// Copyright (c) 2018 Graphcore Ltd. All rights reserved.
#include <algorithm>
#include <memory>
#include <popart/op/transpose.hpp>
#include <popart/opmanager.hpp>
#include <popart/opserialiser.hpp>
#include <popart/tensor.hpp>

namespace popart {

TransposeBaseOp::TransposeBaseOp(const OperatorIdentifier &_opid,
                                 const std::vector<int64_t> &perm_,
                                 const Op::Settings &settings_)
    : Op(_opid, settings_), perm(perm_) {}

view::RegMap TransposeBaseOp::fwdRegMap(InIndex inIndex, OutIndex) const {
  if (inIndex != 0) {
    throw internal_error("[TransposeBaseOp::fwdRegMap] "
                         "Received input index {} but only 0 allowed, "
                         "This for Op {}, ",
                         inIndex,
                         str());
  }
  auto emptyRegion = view::Region::getEmpty(outRank(getOutIndex()));
  auto permutation = getPerm();
  return [permutation, emptyRegion](const view::Region &r) {
    if (r.isEmpty()) {
      return view::Regions(1, emptyRegion);
    }
    return view::Regions(1, r.transpose(permutation));
  };
}

view::RegMap TransposeBaseOp::bwdRegMap(InIndex inIndex, OutIndex) const {
  if (inIndex != 0) {
    throw internal_error("[TransposeBaseOp::bwdRegMap] "
                         "Received input index {} but only 0 allowed, "
                         "This for Op {}, ",
                         inIndex,
                         str());
  }
  auto emptyRegion = view::Region::getEmpty(inRank(getInIndex()));
  auto permutation = generateReversePermutation();
  return [permutation, emptyRegion](const view::Region &r) {
    if (r.isEmpty()) {
      return view::Regions(1, emptyRegion);
    }
    return view::Regions(1, r.transpose(permutation));
  };
}

TransposeOp::TransposeOp(const OperatorIdentifier &_opid,
                         const std::vector<int64_t> &perm_,
                         const Op::Settings &settings_)
    : TransposeBaseOp(_opid, perm_, settings_) {}

std::unique_ptr<Op> TransposeOp::clone() const {
  return std::make_unique<TransposeOp>(*this);
}

std::vector<std::unique_ptr<Op>> TransposeOp::getGradOps() {
  std::vector<std::unique_ptr<Op>> upops;
  upops.emplace_back(std::make_unique<TransposeGradOp>(*this));
  return upops;
}

void TransposeBaseOp::setup() {
  auto in_shape = inInfo(getInIndex()).shape();

  // If perm is empty, set the the default value
  if (perm.empty()) {
    setDefaultPerm();
  } else if (perm.size() != in_shape.size()) {
    throw error(
        "Rank of permutation tensor {}, rank {} must be equal to rank of "
        "input tensor, shape {}, rank {}.",
        perm,
        perm.size(),
        in_shape,
        in_shape.size());
  }

  Shape out_shape;
  for (auto i : perm) {
    out_shape.push_back(in_shape[i]);
  }

  outInfo(getOutIndex()) = {inInfo(getInIndex()).data_type(), out_shape};
}

std::vector<int64_t> TransposeBaseOp::generateReversePermutation() const {
  std::vector<int64_t> reverse_perm(perm.size());
  for (int i = 0; i < perm.size(); i++) {
    reverse_perm[perm[i]] = i;
  }

  return reverse_perm;
}

void TransposeBaseOp::setDefaultPerm() {
  auto in_shape = inInfo(getInIndex()).shape();

  // default behaviour is to reverse the shape of the input tensor
  if (perm.empty()) {
    for (int64_t i = in_shape.size() - 1; i >= 0; i--) {
      perm.push_back(i);
    }
  }
}

void TransposeOp::appendOutlineAttributes(OpSerialiserBase &os) const {
  Op::appendOutlineAttributes(os);
  os.appendAttribute("perm", getPerm());
}

bool TransposeOp::canBeReplacedByIdentity() {
  return std::is_sorted(getPerm().begin(), getPerm().end());
}

std::unique_ptr<Op>
TransposeOp::getInplaceVariant(const OperatorIdentifier &operator_id) const {
  if (operator_id == Onnx::CustomOperators::TransposeInplace) {
    return std::make_unique<TransposeInplaceOp>(*this);
  }
  // catch remaining cases and throw an error
  return Op::getInplaceVariant(operator_id);
}
std::vector<std::tuple<OperatorIdentifier, float>>
TransposeOp::inplacePriorityDefault() const {
  return {{Onnx::CustomOperators::TransposeInplace, 10}};
}

TransposeGradOp::TransposeGradOp(const TransposeOp &fwdOp)
    : TransposeOp(Onnx::GradOperators::TransposeGrad,
                  fwdOp.generateReversePermutation(),
                  fwdOp.getSettings()) {}

std::unique_ptr<Op> TransposeGradOp::clone() const {
  return std::make_unique<TransposeGradOp>(*this);
}

const std::vector<GradInOutMapper> &TransposeGradOp::gradInputInfo() const {
  static const std::vector<GradInOutMapper> inInfo = {
      {getInIndex(), TransposeOp::getOutIndex(), GradOpInType::GradOut}};

  return inInfo;
}

const std::map<int, int> &TransposeGradOp::gradOutToNonGradIn() const {
  static const std::map<int, int> outInfo = {
      {getOutIndex(), TransposeOp::getInIndex()}};

  return outInfo;
}

TransposeInplaceOp::TransposeInplaceOp(const TransposeOp &op)
    : TransposeBaseOp(Onnx::CustomOperators::TransposeInplace,
                      op.getPerm(),
                      op.settings) {}

std::unique_ptr<Op> TransposeInplaceOp::clone() const {
  return std::make_unique<TransposeInplaceOp>(*this);
}

namespace {

static OpDefinition::DataTypes T = {DataType::UINT8,
                                    DataType::UINT16,
                                    DataType::UINT32,
                                    DataType::UINT64,
                                    DataType::INT8,
                                    DataType::INT16,
                                    DataType::INT32,
                                    DataType::INT64,
                                    DataType::FLOAT16,
                                    DataType::FLOAT,
                                    DataType::BOOL};

static OpDefinition
    transposeOpDef({OpDefinition::Inputs({{"data", T}}),
                    OpDefinition::Outputs({{"transposed", T}}),
                    OpDefinition::Attributes({{"perm", {"*"}}})});

static OpCreator<TransposeOp> transposeOpCreator(
    OpDefinitions({
        {Onnx::Operators::Transpose_1, transposeOpDef},
    }),
    [](const OperatorIdentifier &_opid,
       const Op::Settings &settings,
       const Attributes &attr) -> std::unique_ptr<Op> {
      Shape perm = attr.getAttribute<Attributes::Ints>("perm", {});
      return std::unique_ptr<Op>(new TransposeOp(_opid, perm, settings));
    },
    true);
} // namespace

} // namespace popart
