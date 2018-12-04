#include <poponnx/makeunique.hpp>
#include <poponnx/op/subtract.hpp>
#include <poponnx/tensor.hpp>

namespace poponnx {

SubtractOp::SubtractOp(const onnx::NodeProto &node, Ir *_pir)
    : Op(node, _pir) {}

std::unique_ptr<Op> SubtractOp::clone() const {
  return make_unique<SubtractOp>(*this);
}

std::vector<std::unique_ptr<Op>> SubtractOp::getGradOps() {
  std::vector<std::unique_ptr<Op>> upops;

  const auto &shape_a0 = inShape(SubtractOp::getArg0InIndex());
  const auto &shape_o0 = outShape(SubtractOp::getOutIndex());

  upops.emplace_back(make_unique<SubtractArg0GradOp>(
      this, npReductionAxis(shape_a0, shape_o0)));
  upops.emplace_back(make_unique<SubtractArg1GradOp>(this));

  return upops;
}

void SubtractOp::setup() {
  outInfo(SubtractOp::getOutIndex()) =
      npOut(inInfo(getArg0InIndex()), inInfo(getArg1InIndex()));
}

SubtractArg0GradOp::SubtractArg0GradOp(SubtractOp *op_,
                                       const std::vector<int64_t> &_axes)
    : ReduceSumOp({"SubtractArg0Grad", op_->pir, {}, getPoponnxDomain()},
                  _axes,
                  false),
      forward_op_arg_info(op_->inInfo(SubtractOp::getArg0InIndex())) {}

const std::map<int, int> &SubtractArg0GradOp::gradOutToNonGradIn() const {
  static const std::map<int, int> outInfo = {
      {getOutIndex(), SubtractOp::getArg0InIndex()}};

  return outInfo;
}

const std::vector<GradInOutMapper> &SubtractArg0GradOp::gradInputInfo() const {
  static const std::vector<GradInOutMapper> inInfo = {
      {getInIndex(), SubtractOp::getOutIndex(), GradOpInType::GRADOUT}};

  return inInfo;
}

void SubtractArg0GradOp::setup() {
  outInfo(getOutIndex()) = forward_op_arg_info;
}

SubtractArg1GradOp::SubtractArg1GradOp(SubtractOp *op_)
    : Op({"SubtractArg1Grad", op_->pir, {}, getPoponnxDomain()}),
      forward_op_arg_info(op_->inInfo(SubtractOp::getArg1InIndex())) {}

const std::map<int, int> &SubtractArg1GradOp::gradOutToNonGradIn() const {
  static const std::map<int, int> outInfo = {
      {getOutIndex(), SubtractOp::getArg1InIndex()}};

  return outInfo;
}

const std::vector<GradInOutMapper> &SubtractArg1GradOp::gradInputInfo() const {
  static const std::vector<GradInOutMapper> inInfo = {
      {getInIndex(), SubtractOp::getOutIndex(), GradOpInType::GRADOUT}};

  return inInfo;
}

std::unique_ptr<Op> SubtractArg1GradOp::clone() const {
  return make_unique<SubtractArg1GradOp>(*this);
}

void SubtractArg1GradOp::setup() {
  outInfo(getOutIndex()) = forward_op_arg_info;
}

} // namespace poponnx
