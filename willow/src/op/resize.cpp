#include <cmath>

#include <popart/error.hpp>
#include <popart/op/resize.hpp>
#include <popart/opmanager.hpp>
#include <popart/tensor.hpp>

namespace popart {

std::string toString(const ResizeMode &mode) {
  switch (mode) {
  case ResizeMode::Nearest:
    return "nearest";
  case ResizeMode::Linear:
    return "linear";
  case ResizeMode::N:
  default:
    throw error("Bad ResizeMode '{}'", static_cast<int>(mode));
  }
}

std::ostream &operator<<(std::ostream &os, const ResizeMode &x) {
  os << toString(x);
  return os;
}

ResizeOp::ResizeOp(const OperatorIdentifier &opid_,
                   const Op::Settings &settings_,
                   ResizeMode mode_,
                   const std::vector<float> &scales_)
    : Op(opid_, settings_), scales(scales_), mode(mode_) {}

std::unique_ptr<Op> ResizeOp::clone() const {
  return std::make_unique<ResizeOp>(*this);
}
std::vector<std::unique_ptr<Op>> ResizeOp::getGradOps() {
  std::vector<std::unique_ptr<Op>> upops;
  upops.emplace_back(std::make_unique<ResizeGradOp>(*this));
  return upops;
}
void ResizeOp::setup() {
  auto inputShape = inShape(getInIndex());

  // Check the mode
  if (mode != ResizeMode::Nearest) {
    throw error("Resize op only supports the mode 'nearest' at this time.");
  }

  // Sanity check scales
  if (scales.size() != inputShape.size()) {
    throw error(
        "There should be at exactly {inputShape.size()} elements in resize op "
        "inputs, scales. Scales has {scales.size()} elements.");
  }

  Shape outputShape;
  for (int i = 0; i < inputShape.size(); i++) {
    float dim = std::floor(inputShape[i] * scales.at(i));
    outputShape.push_back(static_cast<int64_t>(dim));
  }

  outInfo(getOutIndex()) = {inInfo(getInIndex()).dataType(), outputShape};
}

void ResizeOp::connectInTensor(InIndex inIndex, TensorId tenId) {
  // Ignore all but the first input
  if (inIndex == getInIndex()) {
    Op::connectInTensor(inIndex, tenId);
  }
}

namespace {

// We cant just inverse the forward pass scales because of the floor operation.
//   floor(3 * 2.5) = 7
// but
//   floor(7 * (1 / 2.5)) = 2, not 3
std::vector<float> gradScales(const ResizeOp &op) {
  auto inShape  = op.inShape(ResizeOp::getInIndex());
  auto outShape = op.outShape(ResizeOp::getOutIndex());
  std::vector<float> result;
  for (int i = 0; i < inShape.size(); i++) {
    result.push_back(static_cast<float>(inShape.at(i)) / outShape.at(i));
  }
  return result;
}

} // namespace

ResizeGradOp::ResizeGradOp(const ResizeOp &op_)
    : ResizeOp(Onnx::GradOperators::ResizeGrad,
               op_.getSettings(),
               op_.getMode(),
               gradScales(op_)) {}

const std::vector<GradInOutMapper> &ResizeGradOp::gradInputInfo() const {
  static const std::vector<GradInOutMapper> inInfo = {
      {getInIndex(), ResizeOp::getOutIndex(), GradOpInType::GradOut}};
  return inInfo;
}

const std::map<int, int> &ResizeGradOp::gradOutToNonGradIn() const {
  static const std::map<int, int> outInfo = {
      {getOutIndex(), ResizeOp::getInIndex()}};
  return outInfo;
}

namespace {

ResizeMode getResizeModeFromString(const std::string &mode) {
  static std::map<std::string, ResizeMode> modeMap = {
      {"nearest", ResizeMode::Nearest}, {"linear", ResizeMode::Linear}};
  auto found = modeMap.find(mode);
  if (found != modeMap.end()) {
    return found->second;
  } else {
    throw error("Unrecognised resize mode {}", mode);
  }
}

static OpDefinition::DataTypes T1 = {DataType::UINT8,
                                     DataType::UINT16,
                                     DataType::UINT32,
                                     DataType::INT8,
                                     DataType::INT16,
                                     DataType::INT32,
                                     DataType::FLOAT16,
                                     DataType::FLOAT};
static OpDefinition::DataTypes T2 = {DataType::FLOAT};

static OpDefinition resize10_OpDef({OpDefinition::Inputs({
                                        {"X", T1},
                                        {"scales", T2},
                                    }),
                                    OpDefinition::Outputs({{"Y", T1}}),
                                    OpDefinition::Attributes({
                                        {"mode", {"nearest"}},
                                    })});

static OpCreator<ResizeOp> resize10_OpCreator(
    OpDefinitions({{Onnx::Operators::Resize_10, resize10_OpDef}}),
    [](const OpCreatorInfo &info) -> std::unique_ptr<Op> {
      int xInputIndex      = 0;
      int scalesInputIndex = 1;
      auto scalesTensor    = info.getInputTensor(scalesInputIndex);
      if (!scalesTensor->hasTensorData()) {
        throw error("Scales tensor has no data");
      }
      auto inputTensor = info.getInputTensor(xInputIndex);
      int nelms        = static_cast<int>(inputTensor->info.shape().size());
      std::vector<float> scales;
      if (scalesTensor->info.dataType() == DataType::FLOAT) {
        scales = scalesTensor->tensorData()->copyDataAs<float>(nelms);
      } else if (scalesTensor->info.dataType() == DataType::FLOAT16) {
        auto temp = scalesTensor->tensorData()->copyDataAs<float16_t>(nelms);
        for (auto i : temp) {
          scales.push_back(i);
        }
      } else {
        throw error("Can not import scales from tensor of type {}",
                    scalesTensor->info.dataType());
      }

      ResizeMode mode = ResizeMode::Nearest;
      if (info.attributes.hasAttribute("mode")) {
        std::string modeString =
            info.attributes.getAttribute<Attributes::String>("mode");
        mode = getResizeModeFromString(modeString);
      }

      return std::unique_ptr<Op>(
          new ResizeOp(info.opid, info.settings, mode, scales));
    },
    true);

} // namespace

} // namespace popart
