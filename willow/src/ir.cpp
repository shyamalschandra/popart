#include <algorithm>
#include <array>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <unordered_set>
#include <vector>

#include <poponnx/builder.hpp>
#include <poponnx/ces/constexpr.hpp>
#include <poponnx/ces/onnxconstexpr.hpp>
#include <poponnx/chains.hpp>
#include <poponnx/error.hpp>
#include <poponnx/filereader.hpp>
#include <poponnx/intervals.hpp>
#include <poponnx/ir.hpp>
#include <poponnx/logging.hpp>
#include <poponnx/op/loss.hpp>
#include <poponnx/opmanager.hpp>
#include <poponnx/optimizer.hpp>
#include <poponnx/optionflags.hpp>
#include <poponnx/pbwrap.hpp>
#include <poponnx/scheduler.hpp>
#include <poponnx/tensor.hpp>
#include <poponnx/tensorindex.hpp>
#include <poponnx/tensorinfo.hpp>
#include <poponnx/tensornames.hpp>
#include <poponnx/tensors.hpp>
#include <poponnx/topocons.hpp>
#include <poponnx/util.hpp>

// The transformations
#include <poponnx/transforms/interipucopy.hpp>
#include <poponnx/transforms/prune.hpp>
#include <poponnx/transforms/recompute.hpp>
#include <poponnx/transforms/virtual_graph_check.hpp>

// The layers required to construct the backwards pass
#include <poponnx/op/batchnorm.hpp>
#include <poponnx/op/sum.hpp>
#include <poponnx/op/varupdate.hpp>

#include <poponnx/patterns/inplace.hpp>

namespace poponnx {

Ir::~Ir() = default;

void Ir::confirmNonReservedId(TensorId tenId) const {
  for (auto reservedPrefix : reservedPrefixes()) {
    if (tenId.find(reservedPrefix) != std::string::npos) {
      throw error("Provided tensor " + tenId +
                  " has an invalid name: clash with reserved prefix " +
                  reservedPrefix);
    }
  }
}

GradNonGradPair::GradNonGradPair(Op *g_, Op *ng_) : grad(g_), nongrad(ng_) {}

GradNonGradPair::GradNonGradPair() : GradNonGradPair(nullptr, nullptr) {}

const onnx::ModelProto &Ir::getModel() const { return *onnxModel; }

std::vector<Tensor *> Ir::optimizerTensors() const {
  std::vector<Tensor *> optTensors;
  if (optimizer.get() != nullptr) {
    for (auto &id_info : optimizer->tensorInfos()) {
      optTensors.push_back(getTensors().get(id_info.first));
    }
  }
  return optTensors;
}

// the rule followed : a Stream tensor which is not an
// optimizer tensor is a streamed data tensor
std::vector<Tensor *> Ir::dataStreamTensors() const {
  std::vector<Tensor *> dsTensors;
  std::map<TensorId, TensorInfo> optTensorInfo;
  if (optimizer != nullptr) {
    optTensorInfo = optimizer->tensorInfos();
  }
  for (TensorId id : getTensors().getIds(TensorType::Stream)) {
    if (optTensorInfo.find(id) == optTensorInfo.end()) {
      dsTensors.push_back(getTensors().get(id));
    }
  }
  return dsTensors;
}

void Ir::updateOptimizer(const Optimizer *newOptimizer) {
  if (optimizer.get() == nullptr) {
    throw error("ILE: cannot update optimizer before it is set");
  }
  if (!optimizer->validReplacement(newOptimizer)) {
    throw error("This Optimizer of type " + newOptimizer->type_s() +
                " is not a valid replacement for optimizer of type " +
                optimizer->type_s());
  }
  optimizer = newOptimizer->clone();
  optimizer->resetTensorDatas(this);
}

void Ir::eraseOp(OpId id) {
  auto found = ops.find(id);
  if (found == ops.end()) {
    throw error("ILE: no op " + std::to_string(id) + " to erase");
  }
  ops.erase(id);
}

void Ir::dotCheckpoint(DotCheck check) const {

  if (userOptions.dotChecks.count(check) == 0) {
    return;
  }

  // the full path to the .dot file to be written
  std::string dotfn =
      io::appendDirFn(userOptions.logDir, getDotCheckString(check) + ".dot");

  // the name that an Op has in the .dot file
  auto nodeDotId = [](OpId id) { return "\"n_" + std::to_string(id) + "\""; };

  // the name that a Tensor has in the .dot file.
  auto tensorDotId = [](const TensorId &id) { return '\"' + id + '\"'; };

  logging::ir::info("Writing dot file to {}", dotfn);
  std::ofstream strm;
  strm.open(dotfn, std::ios::out);
  if (!strm.is_open()) {
    throw error("failed to open file `" + dotfn + '\'');
  }

  strm << "digraph net {\n";
  strm << "size=\"6,6\";\n";
  auto scheduledOps = getOpSchedule({});

  // the position in the schedule at which an op runs
  int scheduleIndex = 0;

  // we keep track of which tensors have been defined in the .dot file
  std::set<TensorId> tensorsVisited{};

  auto getNodeColor = [](TensorType type) {
    switch (type) {
    case TensorType::Stream:
      return "\"red\"";
    case TensorType::Const:
      return "\"blue\"";
    case TensorType::Variable:
      return "\"green\"";
    case TensorType::Momentum:
    case TensorType::Unknown:
    case TensorType::ActGrad:
    case TensorType::N:
    default:
      return "\"black\"";
    }
  };

  auto makeNodeIfRequired = [&getNodeColor,
                             &tensorDotId,
                             &strm,
                             &tensorsVisited](const Tensor *tensor) {
    if (tensorsVisited.count(tensor->id) == 0) {
      tensorsVisited.insert(tensor->id);
      strm << tensorDotId(tensor->id) << " [shape= \"egg\", label=\""
           << tensor->info << " c:" << tensor->consumers.getTotal()
           << "\", color = " << getNodeColor(tensor->tensorType()) << "];\n";
    }
  };

  int start = std::max(0, userOptions.firstDotOp);
  int end   = std::min<int>(userOptions.finalDotOp,
                          static_cast<int>(scheduledOps.size()));

  if (!(start < end) && scheduledOps.size() != 0) {
    throw error("Invalid dot range {{}, {}} with schedule of size {}, "
                "as no Ops will be exported to the .dot file",
                userOptions.firstDotOp,
                userOptions.finalDotOp,
                scheduledOps.size());
  }

  for (int i = start; i < end; ++i) {

    auto &n = scheduledOps.at(i);

    // add the .dot entry for a node [shape="box", label=...]
    strm << nodeDotId(n->id) << " [shape= \"box\", label=\"" << scheduleIndex
         << '.' << ' ' << n->opid.type;

    // Add the debug name if present and requested
    if (userOptions.dotOpNames) {
      if (!n->name().empty()) {
        strm << "(" << n->name() << ")";
      } else {
        strm << " (" << n->id << ")";
      }
    }

    strm << "\"];\n";
    ++scheduleIndex;

    // insert the input -> op edges into the .dot file
    for (auto &ind_ten : n->input->tensorMap()) {
      TensorId tenId = ind_ten.second->id;
      makeNodeIfRequired(ind_ten.second);
      strm << tensorDotId(tenId) << " -> " << nodeDotId(n->id) << ';' << '\n';
    }

    // insert the op -> output edges into the .dot file
    for (auto &ind_ten : n->output->tensorMap()) {
      auto tenId = ind_ten.second->id;
      makeNodeIfRequired(ind_ten.second);
      strm << nodeDotId(n->id) << " -> " << tensorDotId(tenId) << ';' << '\n';
      TensorId possibleGradId = getGradId(tenId);
      if (getTensors().contains(possibleGradId)) {
        // strm << "{rank=same; " << tenId << "; " << possibleGradId <<
        // ";}\n";
      }
    }
  }
  strm << '}' << '\n';
  strm.flush();
}

void Ir::confirmNoReservedIds() const {

  auto &onnxGraph = onnxModel->graph();

  for (const auto &in_ : onnxGraph.input()) {
    confirmNonReservedId(in_.name());
  }

  for (const auto &out_ : onnxGraph.output()) {
    confirmNonReservedId(out_.name());
  }

  for (const auto &tenId : inputShapeInfo.getAllTensorIds()) {
    confirmNonReservedId(tenId);
  }
}

IrBundle::IrBundle(const onnx::ModelProto &modelProto_,
                   const InputShapeInfo &inputShapeInfo_,
                   const DataFlow &dataFlow_,
                   const std::vector<Loss *> &losses_,
                   const Optimizer *optimizer_,
                   const SessionOptions &userOptions_,
                   const Patterns &patterns_)
    : modelProto(modelProto_), inputShapeInfo(inputShapeInfo_),
      dataFlow(dataFlow_), losses(losses_), optimizer(optimizer_),
      userOptions(userOptions_), patterns(patterns_) {}

Ir::Ir() : onnxModel(nullptr) {
  up_tensors.reset(new Tensors(*this));
  scheduler.reset(new Scheduler(this));
  topoCons.reset(new TopoCons());
}

void Ir::setOnnxModel(const onnx::ModelProto &model) {
  onnxModel.reset(new onnx::ModelProto(model));
}

void Ir::setDataFlow(const DataFlow &df) {
  // Inference and evaluation modes require an anchor
  if (!canTrain() && df.nAnchors() == 0) {
    throw error("User must specify an anchor tensor when doing inference or "
                "evalulation.");
  } else {
    dataFlow = df;
  }
}

void Ir::setUserOptions(const SessionOptions &flags) { userOptions = flags; }
void Ir::setInputShapeInfo(const InputShapeInfo &info) {
  inputShapeInfo = info;
}

void Ir::setPatterns(const Patterns &p) { patterns = p; }

void Ir::removeIsolatedTensors() { getTensors().removeIsolated(); }

void Ir::setExecutionMode(const ExecutionMode &mode) { executionMode = mode; }

void Ir::setLosses(const std::vector<Loss *> &_losses) {
  losses.clear();
  for (auto &l : _losses) {
    losses.emplace_back(l->clone());
  }
}

void Ir::setOptimizer(const Optimizer *o) {
  if (o) {
    optimizer = o->clone();

    for (auto &id_info : optimizer->tensorInfos()) {
      TensorId id     = id_info.first;
      TensorInfo info = id_info.second;
      getTensors().addStream(id, info);
      optimizer->setTensorData(getTensors().get(id));
    }
  }
}

void Ir::logIr() {
  std::stringstream ss2;
  append(ss2);
  logging::ir::info(ss2.str());
}

void Ir::verifyOpOutputConnectivity() const {
  logging::ir::info("Checking op output tensor producers");

  // Check op output tensor producers
  for (auto &op_pair : ops) {
    auto &op = op_pair.second;

    for (auto &tensor_pair : op->output->tensorMap()) {
      auto t = tensor_pair.second;

      if (!t->hasProducer()) {
        throw error("Tensor {} should have a producer", t->str());
      }

      if (t->getProducer() != op.get()) {
        throw error(
            "Op {} should produce {}, but it's not the assigned producer",
            op->str(),
            t->str());
      }
    }
  }
}

void Ir::verifyOpInputConnectivity() const {
  logging::ir::info("Checking op input tensor consumers");

  // Count the number of times an op consumes its input tensors
  std::map<std::pair<Tensor *, Op *>, int> consumption_count;
  for (auto &op_pair : ops) {
    auto &op = op_pair.second;

    for (auto &tensor_pair : op->input->tensorMap()) {
      auto t = tensor_pair.second;

      consumption_count[{t, op.get()}]++;
    }
  }

  // Check that the consumption count matches the value reported by Consumers::n
  for (auto &cons_count : consumption_count) {
    auto tensor = cons_count.first.first;
    auto op     = cons_count.first.second;
    auto count  = cons_count.second;

    if (tensor->consumers.n(op) != count) {
      throw error("Op {} should consume {} {} times, but it "
                  "consumes it {} times",
                  op->str(),
                  tensor->str(),
                  count,
                  tensor->consumers.n(op));
    }
  }
}

void Ir::verifyTensorProducerConnectivity() const {
  logging::ir::info("Checking tensor producer outputs");

  for (auto &tid : getTensors().getAllTensorIds()) {
    auto tensor = getTensors().get(tid);

    if (tensor->hasProducer() && tensor->tensorType() == TensorType::Stream) {
      auto op = tensor->getProducer();
      throw error("Tensor {} is a stream tensor, but has op {} as a producer",
                  tensor->str(),
                  op->str());
    }

    if (tensor->hasProducer() && tensor->tensorType() == TensorType::Const) {
      auto op = tensor->getProducer();
      throw error("Tensor {} is a const tensor, but has op {} as a producer",
                  tensor->str(),
                  op->str());
    }

    if (tensor->hasProducer() && tensor->tensorType() == TensorType::Variable) {
      auto op = tensor->getProducer();
      throw error("Tensor {} is a variable tensor, but has op {} as a producer",
                  tensor->str(),
                  op->str());
    }

    if (!tensor->hasProducer() && tensor->tensorType() == TensorType::ActGrad) {
      throw error("Tensor {} is an actgrad tensor, but doesn't have a producer",
                  tensor->str());
    }

    // Check that the producer op has the tensor as an output
    if (tensor->hasProducer()) {
      auto op = tensor->getProducer();

      if (op->output == nullptr) {
        throw error("Op {} output tensor index map is null", op->str());
      }

      if (op->output->indices(tensor).empty()) {
        throw error(
            "Tensor {} has op {} as a producer, but it doesn't appear in "
            "the op's outputs",
            tensor->str(),
            op->str());
      }

      if (op->output->indices(tensor).size() > 1) {
        throw error("Tensor {} has op {} as a producer, but it appears in "
                    "the op's outputs {} times",
                    tensor->str(),
                    op->str(),
                    op->output->indices(tensor).size());
      }
    }
  }
}

void Ir::verifyTensorConsumerConnectivity() const {
  logging::ir::info("Checking tensor consumer inputs");

  // Count the number of times a tensor is consumed by an op
  std::map<std::pair<Tensor *, Op *>, int> consumption_count;
  for (auto &tid : getTensors().getAllTensorIds()) {
    auto tensor = getTensors().get(tid);

    for (auto op : tensor->consumers.getOps()) {
      consumption_count[{tensor, op}] += tensor->consumers.n(op);
    }
  }

  // Check that the consumption count matches the value reported by
  // op->input->indices(tensor).size()
  for (auto &cons_count : consumption_count) {
    auto tensor = cons_count.first.first;
    auto op     = cons_count.first.second;
    auto count  = cons_count.second;

    if (op->input == nullptr) {
      throw error("Op {} input tensor index map is null", op->str());
    }

    if (op->input->indices(tensor).size() != count) {
      throw error("Tensor {} should have op {} as a consumer {} times, but it "
                  "consumes it {} times",
                  tensor->str(),
                  op->str(),
                  op->input->indices(tensor).size(),
                  count);
    }
  }
}

void Ir::verifyConnectivity() const {
  logging::ir::info("Checking IR connectivity");

  verifyOpInputConnectivity();
  verifyOpOutputConnectivity();
  verifyTensorProducerConnectivity();
  verifyTensorConsumerConnectivity();

  logging::ir::info("IR connectivity check passed");
}

bool Ir::isCandidateForConstExprFolding(const Tensor &tensor) const {
  auto tt = tensor.tensorType();

  if (canTrain()) {
    if (tt != TensorType::Const) {
      return false;
    }
  } else {
    // evalulation or inference
    if (tt != TensorType::Const && tt != TensorType::Variable) {
      return false;
    }
  }
  return true;
}

std::set<Tensor *> Ir::getRootInputsToOp(Op *op) {
  if (opAndRootInputs.find(op->id) != opAndRootInputs.end()) {
    // We have already stored the root inputs for this op
    // in a map. Retrieve here instead of performing search
    return opAndRootInputs.at(op->id);
  } else {
    std::set<Tensor *> rootInputs;

    // Get input tensors Ids
    std::vector<TensorId> inputIds = getTensors().getNoProducerIds();
    for (Tensor *tensor : op->input->tensors()) {
      if (std::find(inputIds.begin(), inputIds.end(), tensor->id) !=
          inputIds.end()) {
        // Tensor is a root input
        rootInputs.insert(tensor);
      } else {
        for (auto rootInputTensor : getRootInputsToOp(tensor->getProducer())) {
          rootInputs.insert(rootInputTensor);
        }
      }
    }

    // Add what we've found to the IR's map to speed up
    // future searches
    opAndRootInputs.emplace(op->id, rootInputs);

    return rootInputs;
  }
}

// Verify ConstExpr folding has removed input tensors that should
// been removed:
//  - that initializer inputs are removed when possible in
//    inference and eval modes
//  - that constant inputs are removed when possible in all modes
//
// 1. Get only the tensors we care about checking
// 2. For each tensor, get consumers
// 3. For each consumer, find its root input tensors
// 4. Confirm that at least on root input is not a candidate for
//    ConstExpr folding
//
// Note: this doesn't check that ConstExpr folding has removed
// tenosors that it shouldn't have
void Ir::verifyConstExprFolding() {
  for (auto id : getTensors().getNoProducerIds()) {
    Tensor *tensor = getTensors().get(id);

    // 1
    if (!isCandidateForConstExprFolding(*tensor)) {
      continue;
    }

    // 2 & 3
    std::set<Tensor *> rootInputs;
    for (auto consumingOp : tensor->consumers.getOps()) {
      for (auto rootInput : getRootInputsToOp(consumingOp)) {
        rootInputs.insert(rootInput);
      }
    }

    // 4
    bool shouldHaveFoldedTensor = true;
    for (auto rootInput : rootInputs) {
      if (!isCandidateForConstExprFolding(*rootInput)) {
        shouldHaveFoldedTensor = false;
      }
    }
    if (shouldHaveFoldedTensor) {
      logging::ir::warn(
          "ConstExpr folding has failed to remove input tensor {}, even though "
          "none of the root inputs to its consumers are variable tensors",
          tensor->id);
    }
  }
}

void Ir::prepare(const IrBundle &gb) {

  if (isPrepared) {
    throw error("Ir::prepare called more than once");
  }

  // Require gb.losses.empty() => !gb.optimizer
  if (gb.losses.empty() && gb.optimizer) {
    throw error("An optimizer is set without any losses");
  }

  if (gb.optimizer) {
    setExecutionMode(ExecutionMode::TRAINING);
  } else if (gb.losses.empty()) {
    setExecutionMode(ExecutionMode::INFERENCE);
  } else {
    setExecutionMode(ExecutionMode::EVALUATION);
  }

  setDataFlow(gb.dataFlow);
  setUserOptions(gb.userOptions);
  setInputShapeInfo(gb.inputShapeInfo);
  setPatterns(gb.patterns);
  setOnnxModel(gb.modelProto);

  setLosses(gb.losses);

  confirmNoReservedIds();

  registerInputTensors();

  logging::ir::info("Patterns : {}", patterns);
  // todo : validate the selected patterns

  // construct the forward pass from ONNX,
  constructForwards();
  dotCheckpoint(DotCheck::FWD0);
  applyPreAliasPatterns();
  dotCheckpoint(DotCheck::FWD1);

  if (canEvaluate()) {
    growFinalLoss();
    updateVertices();
    setNPathsToLoss();
  }

  // tensors with no producer and no consumers are removed
  // at this point. We may want something more subtle.
  removeIsolatedTensors();

  setOptimizer(gb.optimizer);

  if (canTrain()) {
    constructBackwards();
  }
  updateVertices();
  dotCheckpoint(DotCheck::BWD0);

  // confirm that all the anchor names provided
  // are indeed real tensor names. This is a check
  // that the user has not provided incorrect names.
  // We allow duplicates.
  validateAnchors();
  applyTransform(Prune::id());

  applyPreAliasPatterns();
  setNPathsToLoss();

  // tensors with no producer and no
  // consumers are removed at this point.
  removeIsolatedTensors();
  updateVertices();

  // Explicitly set this transform to default (off),
  // unless either
  // 1. The option is switched on by the user, XOR
  // 2. Ops in the ir have been annotated with the 'recompute'
  //    attribute. At the moment this can only happen if
  //    specified when building the onnx graph using the poponnx
  //    Builder
  if (userOptions.enableAutoRecomputation && hasUserRecomputeOps()) {
    throw error(
        "A mixture of auto and manual recomputaion is currently not supported");
  } else {
    enableTransform(Recompute::id(),
                    userOptions.enableAutoRecomputation ||
                        hasUserRecomputeOps());
    applyTransform(Recompute::id());
  }
  updateVertices();

  // we now start applying topological constraints between
  // Ops directly. First, we ensure that the VarUpdate Ops
  // are the final consumers of the Variable tensors
  if (canTrain()) {
    setVarUpdateCons();
  }

  applyTransform(Prune::id());

  updateVertices();

  // Check to make sure that all or none have assigned to an ipu
  applyTransform(VirtualGraphCheck::id());

  // Add internal ops to copy tensors between ipu's as needed
  applyTransform(InterIpuCopy::id());

  updateVertices();

  dotCheckpoint(DotCheck::PREALIAS);
  // Now, we apply the Patterns which can handle and create
  // topological constraints. Currently, this is only one
  // in-placing Pattern.
  if (patterns.isInPlaceEnabled()) {
    applyInplacePattern();
  }

  updateVertices();

  dotCheckpoint(DotCheck::FINAL);

  logIr();

  // some checks, now that prepare is complete
  for (auto &id_op : ops) {
    if (id_op.second->opid == Onnx::CustomGradOperators::NllGrad) {
      logging::ir::warn("Computing gradient of the probabilities to Nll "
                        "might be less efficient than computing "
                        "pre-probability gradients directly with Pattern "
                        "SoftMaxGradDirect");
    }
  }
  verifyConstExprFolding();
  verifyConnectivity();
  // end of checks

  isPrepared = true;
}

void Ir::resetWeights(const onnx::ModelProto &modelProto) {
  auto &onnxGraph = modelProto.graph();

  for (const auto &initializer : onnxGraph.initializer()) {
    TensorId tenId = initializer.name();
    if (!getTensors().contains(tenId)) {
      throw error("no tensor " + tenId + " in tensors");
    }
    auto tensor = getTensors().get(tenId);
    if (tensor->info != TensorInfo(initializer)) {
      throw error(
          "trying to reset weights using tensor with non matching tensor info");
    }
    tensor->tensorData()->resetData(initializer);
  }
}

void Ir::registerInputTensors() {

  auto &onnxGraph = onnxModel->graph();

  // Log the input tensor names, catch the
  // invalid case where they are repeated
  std::stringstream ss;
  std::set<TensorId> inputIds;
  bool repeatedInput = false;
  TensorId repeater  = "";
  ss << "Registering Input Tensors. ONNX Graph Inputs : [ ";
  for (auto &valueInfo : onnxGraph.input()) {
    TensorId id = valueInfo.name();
    ss << id << " ";
    if (inputIds.count(id) != 0) {
      // already seen, this is not valid. Will throw an error below.
      repeatedInput = true;
      repeater      = id;
    }
    inputIds.insert(id);
  }
  ss << "]";
  logging::debug(ss.str());
  if (repeatedInput) {
    throw error("Invalid ONNX Model : repeated name: ({}) in input list",
                repeater);
  }
  // we create a map of the tensors to their consumers' types
  std::map<TensorId, std::vector<std::string>> consumerTypes;

  // populate consumerTypes
  for (auto &node : onnxGraph.node()) {
    for (int i = 0; i < node.input_size(); ++i) {
      auto found = consumerTypes.find(node.input(i));
      if (found == consumerTypes.end()) {
        consumerTypes[node.input(i)] = {node.op_type() + "@" +
                                        std::to_string(i)};
      } else {
        found->second.push_back(node.op_type() + "@" + std::to_string(i));
      }
    }
  }

  auto logCreationInfo = [&consumerTypes](std::string tensor_type,
                                          TensorId tensorId) {
    std::string consumerString = "";
    auto found                 = consumerTypes.find(tensorId);

    if (found == consumerTypes.end()) {
      consumerString = "with no consumers in the ONNX GraphProto";
    }

    else {
      consumerString = "with consumers [ ";
      for (auto &i : found->second) {
        consumerString += i;
        consumerString += " ";
      }
    }
    consumerString += "]";
    logging::info(
        "Adding {} Tensor {} to Ir {}.", tensor_type, tensorId, consumerString);
  };

  std::set<TensorId> onnxInitializers;

  std::set<TensorId> unusedInitializers;

  for (const auto &initializer : onnxGraph.initializer()) {
    TensorId tenId = initializer.name();
    if (consumerTypes.find(tenId) == consumerTypes.end()) {
      logging::info("Not creating Tensor for unused initializer, {}", tenId);
      unusedInitializers.emplace(tenId);
    } else {
      // If inference or evaluation mode add initializers as constants
      if (getExecutionMode() == ExecutionMode::INFERENCE ||
          getExecutionMode() == ExecutionMode::EVALUATION) {
        logCreationInfo("Constant", tenId);
        getTensors().addConstInit(tenId, &initializer);
      } else {
        logCreationInfo("Variable", tenId);
        getTensors().addVarInit(tenId, &initializer);
      }
      onnxInitializers.emplace(tenId);
    }
  }

  // used onnx inputs which are not initializers are true inputs
  for (auto &valueInfo : onnxGraph.input()) {
    TensorId id = valueInfo.name();
    if (onnxInitializers.count(id) == 0 && unusedInitializers.count(id) == 0) {

      // Should we allow unused stream tensors in the ONNX Model? To be decided.
      bool allowUnusedStreamTensors = true;
      if (consumerTypes.find(id) == consumerTypes.end() &&
          !allowUnusedStreamTensors) {
        throw error(
            "Request to create poponnx Stream Tensor {} failed, "
            "as it has no consumers in the ONNX GraphProto. "
            "If Tensor {} is only used as an input "
            "to a Loss, then it should not be included in the ONNX Model, "
            "but its TensorInfo should be in the InputShapeInfo object passed "
            "to the Ir/Session constructor.",
            id);
      }
      logCreationInfo("Stream", id);
      if (valueInfo.has_type() && valueInfo.type().tensor_type().has_shape()) {
        getTensors().addStream(id, TensorInfo(valueInfo.type()));
      } else {
        getTensors().addStream(id, inputShapeInfo.get(id));
      }
    }
  }

  // other true inputs are for the loss calculation (class labels, etc)
  for (const auto &loss : losses) {
    for (const auto &tenId : loss->getStreamTensorNames()) {
      // another loss might have already registered this tensor
      if (!getTensors().contains(tenId)) {
        getTensors().addStream(tenId, inputShapeInfo.get(tenId));
      } else {
        Tensor *tensorAlreadyPresent = getTensors().get(tenId);
        if (tensorAlreadyPresent->tensorType() != TensorType::Stream) {
          throw error("type mismatch for tensor " + tenId);
        }
      }
    }
  }
}

std::vector<std::set<Op *>>
Ir::getLiveSets(const std::vector<Op *> &topoOps) const {

  // the key op waits for the ops in val
  // so the key op is later in the sort.
  std::map<Op *, std::vector<Op *>> waiting;

  // the number of ops that are waiting for key
  // this is NOT the size of the values of is_waiting_for
  std::map<Op *, int> nWaiting;

  for (Op *op : topoOps) {
    nWaiting[op] = 0;
    waiting[op]  = {};
  }
  for (Op *op : topoOps) {
    for (auto t_inds : op->input->indicesMap()) {
      Tensor *tensor = t_inds.first;
      if (tensor->hasProducer()) {
        Op *prod = tensor->getProducer();
        // have we noted that op is waiting for prod yet? if not,
        if (std::find(waiting[op].begin(), waiting[op].end(), prod) ==
            waiting[op].end()) {
          // make note
          waiting[op].push_back(prod);
          // increase the number of ops waiting for prod
          ++nWaiting[prod];
        }
      }
    }
  }

  std::set<Op *> live = {};
  std::vector<std::set<Op *>> liveSets;
  for (Op *newOp : topoOps) {
    for (Op *isEarlier : waiting[newOp]) {
      if (live.count(isEarlier) == 0) {
        throw error(
            "ILE: op should still be live (newOp waits for its output)");
      }
      --nWaiting[isEarlier];
      if (nWaiting[isEarlier] == 0) {
        live.erase(isEarlier);
      }
    }
    live.insert(newOp);
    liveSets.push_back(live);
  }
  return liveSets;
}

void Ir::validateAnchors() const {
  for (TensorId id : dataFlow.anchors()) {
    if (!getTensors().contains(id)) {
      std::stringstream ss;
      ss << "Anchor tensor `" << id << "' not in tensors. ";
      // add some trouble-shooting for a case I stumbled upon:
      if (id.find(reservedGradientPrefix()) != std::string::npos) {
        std::string degrad = id.substr(reservedGradientPrefix().size());
        if (getTensors().contains(degrad)) {
          ss << "\nInterestingly, `" << degrad << '\'' << " IS in tensors.\n";
          ss << "Note that not all tensors can have their gradients "
             << "anchored:\nif an activation tensor does not lead "
             << "to the loss,\nits gradient is zero and never computed.";
        }
      } else {
        ss << "The tensors are:\n";
        getTensors().append(ss);
      }
      throw error(ss.str());
    }
  }
}

bool Ir::applyPreAliasPattern(const PreAliasPattern *pattern) {
  bool result = false;

  // the pattern chooses what order to go through the ops in

  std::vector<OpId> v_ops;
  v_ops.reserve(ops.size());

  for (auto &id_op : ops) {
    v_ops.push_back(id_op.first);
  }

  for (auto opId : v_ops) {
    auto itr = ops.find(opId);

    // If the op still exists
    if (itr != ops.end()) {
      Op *op = itr->second.get();
      if (pattern->matches(op)) {
        if (!pattern->touchesAnchored(op)) {
          logging::pattern::debug("Applying pattern {} to {}",
                                  pattern->getPatternName(),
                                  op->debugName());
          result |= pattern->apply(op);
        }
      }
    }
  }

  return result;
}

void Ir::applyPreAliasPatterns() {

  bool keepRunning = true;
  std::vector<std::unique_ptr<PreAliasPattern>> pList =
      patterns.getPreAliasList();

  while (keepRunning) {
    foldConstants();

    keepRunning = false;
    for (auto &pattern : pList) {
      keepRunning |= applyPreAliasPattern(pattern.get());
    }
  }
}

void Ir::applyTransform(std::size_t transformId) {
  // Unless explictly set, a transform is enabled
  if (transformEnableMap.count(transformId) == 0 ||
      transformEnableMap.at(transformId)) {
    Transform::applyTransform(transformId, *this);
  }
}

void Ir::enableTransform(std::size_t transformId, bool enable) {
  transformEnableMap[transformId] = enable;
}

std::vector<Op *> Ir::opsOfType(const OperatorIdentifier &opid) {
  std::vector<Op *> typedOps;
  for (auto &id_op : ops) {
    if (id_op.second->opid == opid) {
      typedOps.push_back(id_op.second.get());
    }
  }
  return typedOps;
}

bool Ir::isAnchored(TensorId tenId) { return dataFlow.isAnchored(tenId); }

void Ir::constructForwards() {
  for (const auto &node : onnxModel->graph().node()) {
    if (OnnxConstExprUtil::isConst(node)) {
      OnnxConstExprUtil::processNode(node, this);
    } else {
      Op *op = growFromNode(node);
      // Not necessary to set the phase here (it will be done in
      // updateVertices). To check our logic though, we do this here
      // and then check that we agree in updateVertices()
      if (op) {
        op->setPhase(Phase::FWD);
      }

      // process ops as they are created
      // Reshape requires a const input tensor at creation time
      // if const folding is left till after the ir is completly constructed
      // then Reshape may not get a const input tensor at creation time
      if (ConstExprUtil::isComputable(op, this)) {
        ConstExprUtil::processOp(op, this);
      }
    }
  }
}

void Ir::foldConstants() {
  logging::ir::trace("Folding constants");
  ConstExprUtil::foldConstants(this);
}

std::string reservedGradientPrefix() { return "d__"; }
std::string reservedRecomputePrefix() { return "r__"; }
std::vector<std::string> reservedPrefixes() {
  return {reservedGradientPrefix(), reservedRecomputePrefix()};
}

OpId Ir::getAndIncrOpsCounter() {
  OpId nOps0 = opsCounter;
  ++opsCounter;
  return nOps0;
}

OpId Ir::getOpsCounter() const { return opsCounter; }

bool Ir::hasUserRecomputeOps() const {
  bool hasUserRecomputeOps = false;
  for (auto &id_op : ops) {
    Op *op = id_op.second.get();
    if (op->getRecomputeOutput()) {
      hasUserRecomputeOps = true;
      break;
    }
  }
  return hasUserRecomputeOps;
}

OpId Ir::moveIntoIr(std::unique_ptr<Op> op) {
  OpId id = op->id;
  ops[id] = std::move(op);
  return id;
}

Op *Ir::growGradSumOp(Tensor *target, const std::vector<Tensor *> &toSum) {

  std::unique_ptr<poponnx::Op> gradSum =
      OpManager::createOp(Domain::ai_onnx,
                          "Sum",
                          getOpSetVersionFromModel(Domain::ai_onnx),
                          *this,
                          "GradSum");

  if (getSessionOptions().enableVirtualGraphs) {

    // Count which vgraph's the producer ops are on.
    std::map<int64_t, int64_t> vgraphIdMap;
    for (auto &t : toSum) {
      boost::optional<int64_t> vgraphId = t->getProducer()->getVirtualGraphId();
      if (vgraphId) {
        vgraphIdMap[*vgraphId]++;
      }
    }

    // Find the vgraph id with the most occurrences.
    auto it = std::max_element(vgraphIdMap.begin(),
                               vgraphIdMap.end(),
                               [](const std::pair<int64_t, int64_t> &p1,
                                  const std::pair<int64_t, int64_t> &p2) {
                                 return p1.second < p2.second;
                               });

    gradSum->setVirtualGraphId(it->first);
  }

  OpId opId = moveIntoIr(std::move(gradSum));

  std::vector<TensorId> inputs;
  inputs.reserve(toSum.size());
  for (auto &tensor : toSum) {
    inputs.push_back(tensor->id);
  }
  TensorId gradientId = getGradId(target->id);
  std::vector<TensorId> outputs{gradientId};

  connectInputs(InputVecWrapper(inputs), opId);
  connectOutputs(OutputVecWrapper(outputs), opId);
  Op *op = ops[opId].get();
  op->setup();
  return op;
}

std::vector<Op *> Ir::growGradOps(Op *nonGradOp) {

  OpId nonGradOpId = nonGradOp->id;
  auto backOps     = nonGradOp->getGradOps();
  std::vector<Op *> gradOps;
  for (auto &upop : backOps) {
    Op *gradOp    = upop.get();
    OpId gradOpId = moveIntoIr(std::move(upop));

    // connect inputs of gradOp
    {
      // inputs to gradOp (to populate in this scope):
      std::map<int, std::string> m_inputs;
      //  int max_input_index = 0;
      for (auto &inOutMapper : gradOp->gradInputInfo()) {

        int indexGrad     = inOutMapper.iGrad;
        int indexFwd      = inOutMapper.iNonGrad;
        GradOpInType type = inOutMapper.type;

        //  max_input_index = std::max(indexGrad, max_input_index);

        // the input at index 'indexGrad' to gradOp is
        switch (type) {
        //  (1) the INPUT at index 'indexFwd' of nonGradOp
        case GradOpInType::IN: {
          if (!nonGradOp->input->hasIndex(indexFwd)) {
            throw error("Invalid configuration of gradOp {}. nonGradOp ({}) "
                        "OUTPUT {} is not defined ",
                        gradOp->debugName(),
                        nonGradOp->debugName(),
                        indexFwd);
          }
          m_inputs[indexGrad] = nonGradOp->input->tensor(indexFwd)->id;
          break;
        }

        //  (2) the OUTPUT at index 'indexFwd' of nonGradOp
        case GradOpInType::OUT: {
          if (!nonGradOp->output->hasIndex(indexFwd)) {
            throw error("Invalid configuration of gradOp {}. nonGradOp ({}) "
                        "OUTPUT {} is not defined ",
                        gradOp->debugName(),
                        nonGradOp->debugName(),
                        indexFwd);
          }
          m_inputs[indexGrad] = nonGradOp->output->tensor(indexFwd)->id;
          break;
        }

        //  (3) the GRADIENT of the OUTPUT
        //      at index 'indexFwd' of nonGradOp.
        case GradOpInType::GRADOUT: {
          if (!nonGradOp->output->hasIndex(indexFwd)) {
            std::stringstream ss;
            ss << "No gradient for non-grad-op " << nonGradOp->debugName()
               << " at index " << indexFwd << '.'
               << " Could it be that the path along that index "
               << "did not lead to final loss, "
               << "in which case the gradient is zero?";
            throw error(ss.str());
          }
          m_inputs[indexGrad] =
              getGradId(nonGradOp->output->tensor(indexFwd)->id);
          break;
        }
        }
      }

      connectInputs(InputMapWrapper(m_inputs), gradOpId);
    }

    // connect outputs of gradOp
    {
      std::vector<TensorId> v_outputs;
      for (auto out_in : gradOp->gradOutToNonGradIn()) {
        int gradOut   = out_in.first;
        int nonGradIn = out_in.second;

        if (!nonGradOp->input->tensor(nonGradIn)) {
          throw error("Invalid configuration of gradOp {}. nonGradOp ({}) "
                      "OUTPUT {} is not defined ",
                      gradOp->debugName(),
                      nonGradOp->debugName(),
                      nonGradIn);
        }

        TensorId inId  = nonGradOp->input->tensor(nonGradIn)->id;
        TensorId outId = getEdgeGradId(inId, nonGradOpId, nonGradIn);
        if (v_outputs.size() < gradOut + 1) {
          v_outputs.resize(gradOut + 1, "");
        }
        v_outputs[gradOut] = outId;
      }
      connectOutputs(OutputVecWrapper(v_outputs), gradOpId);
    }
    gradOp->setup();

    // note, as the outputs of gradOp are edge-grad-tensors and not
    // edge-grads, we do not need to match them to non-grad tensors.
    gradOps.push_back(gradOp);
  }

  return gradOps;
}

void TensorGradRegistry::insert(Tensor *nonGrad, Tensor *grad) {
  auto found = partial.find(nonGrad);
  if (found == partial.end()) {
    partial[nonGrad] = {grad};
  } else {
    partial[nonGrad].push_back(grad);
  }
  if (partial[nonGrad].size() == nonGrad->nPathsToLoss()) {
    complete[nonGrad] = partial[nonGrad];
    partial.erase(nonGrad);
  }
}

void OpGradRegistry::insert(Op *nonGrad, int index) {
  auto found = partial.find(nonGrad);
  // so far NO gradients for nonGrad are in:
  if (found == partial.end()) {
    partial[nonGrad] = {};
  }
  // this should be removed when we're happy the IL (internal logic)
  // is correct:
  if (partial[nonGrad].count(index) != 0) {
    throw error("ILE : index already present in OpGradRegistry::insert");
  }
  partial[nonGrad].insert(index);
  // probably just checks that the size of partial is
  // nonGrad->output->n(), but maybe not.
  if (nonGrad->readyToCreateGradients(partial[nonGrad])) {
    complete.push_back(nonGrad);
    partial.erase(nonGrad);
  }
}

std::map<Tensor *, std::vector<Tensor *>> TensorGradRegistry::popComplete() {
  auto toRet = complete;
  complete   = {};
  return toRet;
}

std::vector<Op *> OpGradRegistry::popComplete() {
  auto toRet = complete;
  complete   = {};
  return toRet;
}

// design choice: we could have an "irHasChanged"
// flag which is set to true whenever the Ir changes,
// and then if irHasChanged is false, calls
// to this (and other) functions can do nothing.
// The cost of maintaining irHasChanged is non-trivial
// and would require runtime overhead, for now not
// going to implement it.

void Ir::updateVertices() {

  // for all vertices (Ops and Tensors),
  // what phase is it in (FWD, BWD, LOSS) ?

  // for all vertices (Ops and Tensors),
  // is there a path to a BWD vertex? (YES, NO)

  // determine the phase of all Ops
  for (auto &id_op : ops) {
    Op *op = id_op.second.get();

    // There are several potential sources of information
    // that can be used to determine the Phase of an Op.
    // We gather all such sources, and confirm that they
    // are in agreement.
    std::vector<Phase> suggestions;

    // source 1 : if the op already has a
    // phase set, it should be the same.
    Phase prevPhase = op->getPhase();
    if (prevPhase != Phase::UNDEFINED) {
      suggestions.push_back(prevPhase);
    }

    // source 2 : if a producer of the op's
    // inputs is BWD, then it must be BWD too.
    for (auto tensor_indices : op->input->indicesMap()) {
      Tensor *inTensor = tensor_indices.first;
      if (inTensor->hasProducer()) {
        if (inTensor->getProducer()->getPhase() == Phase::BWD) {
          suggestions.push_back(Phase::BWD);
        }
      }
    }

    // source 3 : if any of the consumers of the
    // op's outputs is FWD, then it must be FWD too.
    for (auto tensor_indices : op->output->indicesMap()) {
      Tensor *outTensor = tensor_indices.first;
      for (Op *consumer : outTensor->consumers.getOps()) {
        if (consumer->getPhase() == Phase::FWD) {
          suggestions.push_back(Phase::FWD);
        }
      }
    }

    // source 4 : if the op is inherits from the
    // LossOp base class, then it is LOSS.
    if (op->isLossOp()) {
      suggestions.push_back(Phase::LOSS);
    }

    // source 5: if the output is "finalLoss", then it is LOSS
    if (op->output->hasIndex(0) && op->output->id(0) == getFinalLossId()) {
      suggestions.push_back(Phase::LOSS);
    }

    // source 6 : if an input or an output has a gradient
    // or recompute prefix, it is BWD
    std::vector<TensorId> insNouts;
    for (auto tensor_indices : op->output->indicesMap()) {
      insNouts.push_back(tensor_indices.first->id);
    }
    for (auto tensor_indices : op->input->indicesMap()) {
      insNouts.push_back(tensor_indices.first->id);
    }
    for (auto id : insNouts) {
      if ((id.find(reservedGradientPrefix()) != std::string::npos) ||
          (id.find(reservedRecomputePrefix()) != std::string::npos)) {
        suggestions.push_back(Phase::BWD);
      }
    }

    if (suggestions.size() == 0) {
      // no suggestions, it must a FWD (assuming all
      // tensors in backwards hace a gradient or
      // recompute prefix in them)
      op->setPhase(Phase::FWD);
    } else {
      for (auto phase : suggestions) {
        if (phase != suggestions[0]) {
          std::stringstream ss;
          ss << "failed to determine phase of " + op->debugName() +
                    ", which has suggested phases: ";
          std::vector<std::string> suggestions_s;
          for (auto &x : suggestions) {
            suggestions_s.push_back(phase_names().at(x));
          }
          appendSequence(ss, suggestions_s);
          throw error(ss.str());
        }
      }
      op->setPhase(suggestions[0]);
    }
  }

  // now we set the tensor phases,
  // as the phase of the earliest
  // consumer or producer
  for (auto &id_op : ops) {
    Op *op = id_op.second.get();
    std::vector<Tensor *> associated_tensors;

    for (auto tensor_indices : op->output->indicesMap()) {
      associated_tensors.push_back(tensor_indices.first);
    }

    for (auto tensor_indices : op->input->indicesMap()) {
      associated_tensors.push_back(tensor_indices.first);
    }

    for (Tensor *tensor : associated_tensors) {
      auto ass_ops = tensor->associatedOps();
      if (ass_ops.size() == 0) {
        throw error("Tensor " + tensor->id + " has no associated ops");
      }
      // starting with the latest of the phases (BWD),
      // update whenever an associated op is in an earlier phase.
      tensor->setPhase(Phase::BWD);
      for (auto ass_op : ass_ops) {
        // FWD is the earliest Phase, if any associated Op is
        // in the FWD phase then so is this tensor
        if (ass_op->getPhase() == Phase::FWD) {
          tensor->setPhase(Phase::FWD);
        } else if (ass_op->getPhase() == Phase::LOSS &&
                   tensor->getPhase() == Phase::BWD) {
          tensor->setPhase(Phase::LOSS);
        }
      }
    }
  }

  // All phases now set.

  // Now, set if there is a path to a bwd op.
  // we do this starting from scratch.

  std::set<Op *> s_op_front;
  std::vector<Op *> v_op_front;

  // initialising all Ops and Vertices to NO
  for (auto &id_op : ops) {
    Op *op = id_op.second.get();
    op->setPathToBwd(PathToBwd::NO);
    for (auto &tensor_indices : op->input->indicesMap()) {
      tensor_indices.first->setPathToBwd(PathToBwd::NO);
    }
    for (auto &tensor_indices : op->output->indicesMap()) {
      tensor_indices.first->setPathToBwd(PathToBwd::NO);
    }
  }

  // initialising all backward and loss
  // Ops to YES, adding them to the front
  for (auto &id_op : ops) {
    Op *op = id_op.second.get();
    if (op->getPhase() == Phase::BWD || op->getPhase() == Phase::LOSS) {
      op->setPathToBwd(PathToBwd::YES);
      v_op_front.push_back(op);
      s_op_front.insert(op);
    }
  }

  while (v_op_front.size() != 0) {
    Op *onPath = v_op_front.back();
    v_op_front.resize(v_op_front.size() - 1);
    s_op_front.erase(onPath);
    for (auto &tensor_indices : onPath->input->indicesMap()) {
      Tensor *tOnPath = tensor_indices.first;
      tOnPath->setPathToBwd(PathToBwd::YES);
      if (tOnPath->hasProducer()) {
        Op *producer = tOnPath->getProducer();
        producer->setPathToBwd(PathToBwd::YES);
        if (s_op_front.count(producer) == 0) {
          s_op_front.insert(producer);
          v_op_front.push_back(producer);
        }
      }
    }
  }
}

void Ir::setNPathsToLoss() {
  auto found = ops.find(finalLossId);
  if (found == ops.end()) {
    // There will be no losses at all for an inference
    return;
  }
  Op *finalLossOp = found->second.get();

  // initialize number of paths for
  // all Ops and Tensors to loss to be zero
  for (auto &id_op : ops) {
    Op *op = id_op.second.get();
    op->setNPathsToLossToZero();
    for (auto t_inds : op->input->indicesMap()) {
      t_inds.first->setNPathsToLossToZero();
    }
    for (auto t_inds : op->output->indicesMap()) {
      t_inds.first->setNPathsToLossToZero();
    }
  }

  std::vector<Op *> opFront{finalLossOp};
  std::set<Op *> opsSeen{finalLossOp};
  std::set<Tensor *> tensorsSeen{};
  while (opFront.size() != 0) {
    Op *op = opFront.back();
    opFront.resize(opFront.size() - 1);
    for (auto &ind_ten : op->input->tensorMap()) {
      auto tensor = ind_ten.second;
      tensor->incrNPathsToLoss();
      if (tensorsSeen.count(tensor) == 0) {
        tensorsSeen.insert(tensor);
        if (tensor->hasProducer()) {
          auto producer = tensor->getProducer();
          producer->incrNPathsToLoss();
          if (opsSeen.count(producer) == 0) {
            opFront.push_back(producer);
            opsSeen.insert(producer);
          }
        }
      }
    }
  }
}

void Ir::constructBackwards() {

  logging::ir::info("constructing backwards pass");

  // definition: edge-gradient. What is output by a grad-op,
  // and which will be summed with other edge-gradients to create
  // a gradient. It is possible that an edge-gradient has the same
  // value as a gradient, if a tensor has only 1 consumer.

  // design decision w.r.t. lambda functions in this function:
  // see-sawing between lambda functions (see two following here)
  // and member functions. In general I don't like lambda functions,
  // their return types are not easily visible and capturing parameters
  // is tedious. However, I also don't like having class variables
  // which are only used in one bit of functionality, because it becomes
  // unclear whether they should be maintained in a valid state throughout
  // the objects life. In this case, I think the second is worse, so
  // going for the lambda solution.
  TensorGradRegistry tensor_grad_registry;
  OpGradRegistry op_grad_registry;

  // signal that a grad-op has created edge-gradients
  auto registerOpGrads = [&tensor_grad_registry](Op *gradOp, Op *nonGradOp) {
    for (auto &index_tensor : gradOp->output->tensorMap()) {
      int opOutInd     = index_tensor.first;
      Tensor *partGrad = index_tensor.second;
      // what input index of nonGradOp does the
      // edge-gradient correspond to?
      int nonGradInInd      = gradOp->getNonGradInIndex(opOutInd);
      Tensor *nonGradTensor = nonGradOp->input->tensor(nonGradInInd);
      tensor_grad_registry.insert(nonGradTensor, partGrad);
    }
  };

  // communicate that a new gradient tensor
  // (which is a sum along edges) is ready
  auto registerTensorGrad = [this, &op_grad_registry](Tensor *sum) {
    Tensor *nonGrad = getTensors().get(getNonGradId(sum->id));
    if (nonGrad->hasProducer()) {
      Op *producer = nonGrad->getProducer();
      // the index at which nonGrad was produced
      int index = producer->output->indices(nonGrad).at(0);
      op_grad_registry.insert(producer, index);
    }
  };

  // grad-ops which have created edge-gradients, but the
  // edge-gradients haven't signalled their existance.
  // initialised as the gradients of the individual losses
  std::vector<GradNonGradPair> opsToRegister = growLossGradients();

  while (!opsToRegister.empty()) {

    registerOpGrads(opsToRegister.back().grad, opsToRegister.back().nongrad);

    opsToRegister.resize(opsToRegister.size() - 1);

    for (auto &nongrad_egrads : tensor_grad_registry.popComplete()) {

      Tensor *nongrad                     = nongrad_egrads.first;
      const std::vector<Tensor *> &egrads = nongrad_egrads.second;
      // nongrad required below, as the name of the output of the
      // created op (sumOp) will be based off of it. Also, we
      // register the link between sumOp's output and nongrad
      Op *sumOp = growGradSumOp(nongrad, egrads);

      // Not necessary to set the phase here (it will be done in
      // updateVertices). To check our logic though, we do this here
      // and then check that we agree in updateVertices()
      sumOp->setPhase(Phase::BWD);

      switch (nongrad->tensorType()) {

      // if sumOp creates the gradient of an activation tensor,
      case TensorType::ActGrad: {
        registerTensorGrad(sumOp->output->tensor(0));
        break;
      }
      case TensorType::Variable: {
        // nothing to do, variable updates
        // follows at the end of this function
        break;
      }
      case TensorType::Stream: {
        // if the user wants the gradient of the
        // input data (unusual case) maybe we won't
        // break here. Example case : generating adversarials
        break;
      }
      case TensorType::Const: {
        break;
      }
      case TensorType::Momentum:
      case TensorType::Unknown:
      case TensorType::N:
        throw error("can't currently register gradient of " +
                    nongrad->tensor_type() + " tensor, " + nongrad->str());

      default: {
        throw error("only handling ActGrad and Variable for now");
      }
      }
    }

    for (Op *op : op_grad_registry.popComplete()) {
      for (auto &gradOp : growGradOps(op)) {
        opsToRegister.push_back({gradOp, op});
      }
    }
  }

  // add weight update ops (we are ignoring momentums for now)
  for (auto &varId : getTensors().getIds(TensorType::Variable)) {

    VariableTensor *tensor =
        dynamic_cast<VariableTensor *>(getTensors().get(varId));
    switch (tensor->getVariableUpdateType()) {
    case VariableUpdateType::Copy:
      // Updates the var by copying it from another tensor
      growCopyVarUpdateOp(varId, tensor->getCopyFromTensor());
      break;
    case VariableUpdateType::Gradient:
      // Updates the var by looking for the matching gradient
      growGradientVarUpdateOp(varId);
      break;
    case VariableUpdateType::None:
    default:
      throw error("Unknown variable update approach");
    };
  }
}

Op *Ir::growCopyVarUpdateOp(TensorId varId, TensorId from) {
  OpId opId =
      moveIntoIr(std::unique_ptr<Op>(new CopyVarUpdateOp(varId, {*this, ""})));

  // The order of inputs is important
  std::vector<TensorId> inputs{varId, from};
  connectInputs(InputVecWrapper(inputs), opId);

  return growVarUpdateOpInternal(opId);
}

Op *Ir::growGradientVarUpdateOp(TensorId varId) {

  // A sanity check that the Tensor is not fixed point type
  if (getTensors().get(varId)->info.getDataTypeInfo()->isFixedPoint()) {
    throw error("Currently only floating point variable tensors are updatable");
  }

  OpId opId   = moveIntoIr(optimizer->createOp(varId, this));
  auto inputs = optimizer->getInputIds(varId);
  connectInputs(InputVecWrapper(inputs), opId);

  return growVarUpdateOpInternal(opId);
}

Op *Ir::growVarUpdateOpInternal(OpId opId) {

  Op *op = ops[opId].get();

  // there are no outputs of var-op
  std::vector<TensorId> outputs{};
  connectOutputs(OutputVecWrapper(outputs), opId);
  op->setup();

  // Not necessary to set the phase here (it will be done in
  // updateVertices). To check our logic though, we do this here
  // and then check that we agree in updateVertices()
  op->setPhase(Phase::BWD);

  trainTargetOps.insert(op);

  return op;
}

void Ir::setVarUpdateCons() {

  for (auto &varId : getTensors().getIds(TensorType::Variable)) {
    // impose the constraint that the varupdates
    // are the last consumers of the vars
    Tensor *var = getTensors().get(varId);

    // we first determine which consumer
    // is the updater. It is the void Op
    Op *varupdater = nullptr;
    for (Op *consumer : var->consumers.getOps()) {
      if (consumer->output->n() == 0) {
        varupdater = consumer;
        break;
      }
    }
    if (varupdater == nullptr) {
      throw error("Failed to determine updater of " + var->id);
    }

    // set the constraints
    for (Op *consumer : var->consumers.getOps()) {
      if (consumer != varupdater) {
        topoCons->insert(consumer, varupdater);
      }
    }
  }
}

Op *Ir::growFromNode(const Node &node) {

  OpId opId = moveIntoIr(addOp(node));

  connectInputs(node, opId);
  connectOutputs(node, opId);

  // finally, set the output tensor info for the output
  // tensors, and any other Op specific class variables
  Op *fromNodeOp = ops[opId].get();
  fromNodeOp->setup();
  return fromNodeOp;
}

void Ir::growFinalLoss() {
  // There may be no losses (in inference especially)
  if (losses.size() == 0) {
    return;
  }

  logging::ir::info("growing final loss");

  std::vector<Op *> lossOps;
  // first, grow each of the individual losses from the user
  for (auto &loss : losses) {
    OpId opId = moveIntoIr(loss->getOp({*this, ""}));
    connectInputs(*loss, opId);
    connectOutputs(*loss, opId);
    Op *lossOp = ops[opId].get();
    lossOps.push_back(lossOp);
    lossOp->setup();

    // Not necessary to set the phase here (it will be done in
    // updateVertices). To check our logic though, we do this here
    // and then check that we agree in updateVertices()
    lossOp->setPhase(Phase::LOSS);
  }

  // now growing the FINAL loss (sum of individual losses)
  std::unique_ptr<poponnx::Op> finalLossSum =
      OpManager::createOp(Domain::ai_onnx,
                          "Sum",
                          getOpSetVersionFromModel(Domain::ai_onnx),
                          *this,
                          "FinalLoss");

  if (getSessionOptions().enableVirtualGraphs) {

    // Count which vgraph's the producer ops are on.
    std::map<int64_t, int64_t> vgraphIdMap;
    for (auto &l : lossOps) {
      boost::optional<int64_t> vgraphId = l->getVirtualGraphId();
      if (vgraphId) {
        vgraphIdMap[*vgraphId]++;
      }
    }

    // Find the vgraph id with the most occurrences.
    auto it = std::max_element(vgraphIdMap.begin(),
                               vgraphIdMap.end(),
                               [](const std::pair<int64_t, int64_t> &p1,
                                  const std::pair<int64_t, int64_t> &p2) {
                                 return p1.second < p2.second;
                               });

    finalLossSum->setVirtualGraphId(it->first);
  }

  OpId opId = moveIntoIr(std::move(finalLossSum));

  std::vector<TensorId> inputs;
  inputs.reserve(lossOps.size());
  for (auto &op : lossOps) {
    // Assume that tensor(0) is always valid
    inputs.push_back(op->output->tensor(0)->id);
  }
  std::vector<TensorId> outputs{getFinalLossId()};
  connectInputs(InputVecWrapper(inputs), opId);
  connectOutputs(OutputVecWrapper(outputs), opId);
  ops[opId]->setup();

  // Not necessary to set the phase here (it will be done in
  // updateVertices). To check our logic though, we do this here
  // and then check that we agree in updateVertices()
  ops[opId]->setPhase(Phase::LOSS);
  finalLossId = opId;
}

TensorId Ir::getFinalLossId() const { return "finalLoss"; }

template <typename T> void Ir::connectInputs(const T &inContainer, OpId opId) {
  Op *op = ops[opId].get();
  for (int inIndex = 0; inIndex < inContainer.input_size(); ++inIndex) {
    auto &inName = inContainer.input(inIndex);
    if (inName == "") {
      // no input at this position
    } else {
      if (!getTensors().contains(inName)) {
        throw error("input " + inName + " should already be in tensor map");
      } else {
        // default: connects tensor <-> op, in both directions.
        // Note that this is a virtual function, and so specific Ops
        // may to do something different to the default here.
        op->connectInTensor(inIndex, inName);
      }
    }
  }
}

void Ir::connectInputsFromInputMapWrapper(const InputMapWrapper &in, OpId id) {
  connectInputs(in, id);
}

void Ir::connectOutputsFromOutputMapWrapper(const OutputMapWrapper &out,
                                            OpId id) {
  connectOutputs(out, id);
}

template <typename T>
void Ir::connectOutputs(const T &outContainer, OpId opId) {
  for (int outIndex = 0; outIndex < outContainer.output_size(); ++outIndex) {
    auto &outName = outContainer.output(outIndex);
    if (outName == "") {
      // no output at this position
    } else {
      // ONNX specifies that a tensor is the output of at most 1 node.
      // here we create the Output (activation or gradient) Tensor and
      // connect it to the Op.
      ops[opId]->createAndConnectOutTensor(outIndex, outName);
    }
  }
}

void Ir::append(std::stringstream &ss) {
  for (auto &op : getOpSchedule({})) {
    op->append(ss);
  }
}

int Ir::getDefaultOpsetVersion(const std::string &domain) const {
  if (domain == Domain::ai_onnx) {
    return defaultAiOnnxOpset;
  } else if (domain == Domain::ai_onnx_ml) {
    return defaultAiOnnxMlOpset;
  } else if (domain == Domain::ai_graphcore) {
    return defaultAiGraphcoreOpset;
  } else {
    throw error("No default opset version defined for domain \'{}\'", domain);
  }
}

int Ir::getOpSetVersionFromModel(const std::string &node_domain) const {

  // If the node.domain is blank it means the default ai.onnx
  auto domain = node_domain;
  if (domain == "") {
    domain = Domain::ai_onnx;
  }

  // Get the version of the opset from the model based on the domain
  int version    = 0;
  auto opsetList = getModel().opset_import();
  for (auto &opset : opsetList) {

    std::string opset_domain;
    if (opset.has_domain() == false || opset.domain() == "") {
      opset_domain = Domain::ai_onnx;
    } else {
      opset_domain = opset.domain();
    }

    if (domain == opset_domain) {

      auto opset_version = static_cast<int>(opset.version());

      // If the same domain is mentioned multiple times find the largest
      if (opset_version > version)
        version = opset_version;
    }
  }

  // If the version has not be set use the default
  if (version == 0) {
    version = getDefaultOpsetVersion(domain);
  }

  return version;
}

std::unique_ptr<Op> Ir::addOp(const Node &node) {

  int version = getOpSetVersionFromModel(node.domain());

  std::unique_ptr<Op> p = OpManager::createOp(node.domain(),
                                              node.op_type(),
                                              version,
                                              *this,
                                              node.name(),
                                              node.attribute());
  if (p != nullptr)
    return p;
  else {
    if (node.op_type() == Onnx::AiOnnx::OpSet9::Constant.type) {
      throw error("ILE. Constant Ops are not to be added");
    } else {
      throw error("No class for {}.{}:{}",
                  (node.domain() == "" ? Domain::ai_onnx : node.domain()),
                  node.op_type(),
                  version);
    }
  }
}

std::vector<GradNonGradPair> Ir::growLossGradients() {
  std::vector<GradNonGradPair> pairs;
  if (ops.find(finalLossId) != ops.end()) {
    for (auto &t_inds : getOp(finalLossId)->input->indicesMap()) {
      Tensor *t  = t_inds.first;
      Op *lossOp = t->getProducer();
      for (Op *gradOp : growGradOps(lossOp)) {
        pairs.push_back({gradOp, lossOp});
      }
    }
  }
  return pairs;
}

OpId Ir::getFinalLossOpId() const { return finalLossId; }

Op *Ir::getOp(OpId opId) {
  auto found = ops.find(opId);
  if (found == ops.end()) {
    throw error("No Op `" + std::to_string(opId) + "'");
  }
  return found->second.get();
}

std::vector<Op *> Ir::getOpSchedule(const OpsBeforeKey &gCons) const {
  auto sorted = scheduler->getPartialOpSchedule(gCons);
  if (sorted.size() != ops.size()) {
    throw error("failure to sort topologically in getOpSchedule");
  }
  return sorted;
}

// Are the Ops with all the dependencies a DAG?
bool Ir::isSchedulable(const OpsBeforeKey &gCons) const {
  auto sorted = scheduler->getPartialOpSchedule(gCons);
  if (sorted.size() != ops.size()) {
    return false;
  }
  return true;
}

Ir::ExecutionMode Ir::getExecutionMode() const { return executionMode; }

bool Ir::canInfer() const {
  return getExecutionMode() == ExecutionMode::INFERENCE || canEvaluate();
}

bool Ir::canEvaluate() const {
  return getExecutionMode() == ExecutionMode::EVALUATION || canTrain();
}

bool Ir::canTrain() const {
  return getExecutionMode() == ExecutionMode::TRAINING;
}

bool Ir::containsInitialisers() {
  return !(onnxModel->graph().initializer().empty());
}

void Ir::applyInplacePattern() {

  Inplace inplace;

  // <0> the id of the Op to inplace
  // <1> the type of the inplace Op
  // <2> the priority of this inplacing
  using Triplet = std::tuple<OpId, OperatorIdentifier, float>;

  std::vector<Triplet> priorities;
  for (auto &id_op : ops) {
    Op *op = id_op.second.get();

    // first see if the user has overriden the default priorities
    std::set<OpType> prioritized;
    for (auto ip : op->settings.inplacePriorityVeto) {
      OpType inplaceId = std::get<0>(ip);
      priorities.push_back({
          op->id,
          {
              Domain::ai_graphcore, // the domain (same for all inplace ops)
              inplaceId,            // the name of the Operator (OpId)
              1                     // version
          },
          std::get<1>(ip) // the priority value
      });
      prioritized.insert(inplaceId);
    }

    // for all the inplacers not in the user list, take the default
    for (auto ip : op->inplacePriorityDefault()) {
      OperatorIdentifier identifier = std::get<0>(ip);
      if (prioritized.count(identifier.type) == 0) {
        priorities.push_back({op->id, identifier, std::get<1>(ip)});
      }
    }
  }

  auto tripletComparitor = [](const Triplet &a, const Triplet &b) {
    return std::get<2>(a) > std::get<2>(b);
  };

  if (priorities.size() != 0) {

    // sort in decreasing order of priority,
    std::sort(priorities.begin(), priorities.end(), tripletComparitor);

    // removing all negative priorities. We use std::lower_bound
    // instead of std::find_if, taking advantage of the fact that priorities
    // are sorted at this point.

    // (1) we create a "pivot" with priority 0
    Triplet zeroPriority      = priorities[0];
    std::get<2>(zeroPriority) = 0.;

    // (2) we find the first elememts in priorities which is not less than the
    // pivot, and erase all elements from there to the end. Note that
    // priority 0 elements will be removed.
    auto found = std::lower_bound(
        priorities.begin(), priorities.end(), zeroPriority, tripletComparitor);
    priorities.erase(found, priorities.end());

    // we keep track of which ops have already been inplaced
    std::set<OpId> inplacedAlready;

    for (auto &ip : priorities) {
      OpId id                       = std::get<0>(ip);
      OperatorIdentifier identifier = std::get<1>(ip);
      // first check that the op has not already been inplaced:
      auto inplaced_already_it = inplacedAlready.find(id);
      if (inplaced_already_it != inplacedAlready.end()) {
        // the Op has already been inplaced
      } else {
        Op *op              = ops.at(id).get();
        bool touchesAnchors = false;
        for (auto &tensor : inplace.touches(op, identifier)) {
          if (op->getIr().isAnchored(tensor->id)) {
            touchesAnchors = true;
          }
        }
        if (!touchesAnchors) {
          auto newTopoCons = inplace.getNewTopoCons(op, identifier);
          if (isSchedulable(newTopoCons)) {
            inplace.apply(op, identifier, newTopoCons);
            inplacedAlready.insert(op->id);
          }
        }
      }
    }
  }
}

} // namespace poponnx
