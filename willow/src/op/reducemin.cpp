#include <algorithm>
#include <memory>
#include <popart/op/reducemin.hpp>
#include <popart/opmanager.hpp>
#include <popart/opserialiser.hpp>
#include <popart/tensor.hpp>

namespace popart {

ReduceMinOp::ReduceMinOp(const OperatorIdentifier &_opid,
                         const std::vector<int64_t> &axes_,
                         const int64_t keepdims_,
                         const Op::Settings &settings_)
    : ReduceOp(_opid, axes_, keepdims_, settings_) {}

std::unique_ptr<Op> ReduceMinOp::clone() const {
  return std::make_unique<ReduceMinOp>(*this);
}

std::vector<std::unique_ptr<Op>> ReduceMinOp::getGradOps() {
  std::vector<std::unique_ptr<Op>> result;
  result.emplace_back(std::make_unique<ReduceMinGradOp>(*this, backward_shape));
  return result;
}

ReduceMinGradOp::ReduceMinGradOp(const ReduceMinOp &fwdOp,
                                 const Shape &backward_shape_)
    : ReduceGradOp(Onnx::GradOperators::ReduceMinGrad, fwdOp, backward_shape_) {
}

std::unique_ptr<Op> ReduceMinGradOp::clone() const {
  return std::make_unique<ReduceMinGradOp>(*this);
}

const std::vector<GradInOutMapper> &ReduceMinGradOp::gradInputInfo() const {
  static const std::vector<GradInOutMapper> inInfo = {
      {getInIndex(), ReduceMinOp::getOutIndex(), GradOpInType::GRADOUT},
      {getFwdInInIndex(), ReduceMinOp::getInIndex(), GradOpInType::IN},
      {getFwdOutInIndex(), ReduceMinOp::getOutIndex(), GradOpInType::OUT}};
  return inInfo;
}

namespace {
// @SL@ the new factory method for the reduceMin op will get the attributes from
// the model and pass them to the constructor of the OP
static OpCreator<ReduceMinOp> reduceMinOpCreator(
    {Onnx::Operators::ReduceMin_1, Onnx::Operators::ReduceMin_11},
    [](const OperatorIdentifier &_opid,
       const Op::Settings &settings,
       const Attributes &attr) -> std::unique_ptr<Op> {
      int64_t keepdims = attr.getAttribute<Attributes::Int>("keepdims", 1);
      std::vector<int64_t> axes =
          attr.getAttribute<Attributes::Ints>("axes", {});

      return std::unique_ptr<Op>(
          new ReduceMinOp(_opid, axes, keepdims, settings));
    },
    true);
} // namespace

} // namespace popart
