#include <poponnx/op/elementwise.hpp>
#include <poponnx/tensor.hpp>

namespace poponnx {

ElementWiseUnaryOp::ElementWiseUnaryOp(const OperatorIdentifier &_opid,
                                       Ir *_ir,
                                       const std::string &name,
                                       const Attributes &_attr)
    : Op(_opid, _ir, name, _attr) {}

void ElementWiseUnaryOp::setup() {
  outInfo(getOutIndex()) = inInfo(getInIndex());
}

ElementWiseNonLinearUnaryGradOp::ElementWiseNonLinearUnaryGradOp(
    const OperatorIdentifier &_opid,
    Ir *_ir)
    : Op(_opid, _ir) {}

const std::vector<GradInOutMapper> &
ElementWiseNonLinearUnaryGradOp::gradInputInfo() const {
  static const std::vector<GradInOutMapper> inInfo = {
      {getGradInIndex(),
       ElementWiseUnaryOp::getOutIndex(),
       GradOpInType::GRADOUT},
      {getFwdArgInIndex(), ElementWiseUnaryOp::getInIndex(), GradOpInType::IN}};

  return inInfo;
}

const std::map<int, int> &
ElementWiseNonLinearUnaryGradOp::gradOutToNonGradIn() const {
  static const std::map<int, int> outInfo = {
      {getOutIndex(), ElementWiseUnaryOp::getInIndex()}};

  return outInfo;
}

void ElementWiseNonLinearUnaryGradOp::setup() {
  outInfo(getOutIndex()) = inInfo(getFwdArgInIndex());
}

} // namespace poponnx
