#ifndef GUARD_NEURALNET_SIGMOID_HPP
#define GUARD_NEURALNET_SIGMOID_HPP

#include <poponnx/op/elementwise.hpp>

namespace poponnx {

class SigmoidOp : public ElementWiseUnaryOp {
public:
  SigmoidOp(const OperatorIdentifier &_opid, const Op::Settings &settings_);
  std::unique_ptr<Op> clone() const override;
  std::vector<std::unique_ptr<Op>> getGradOps() final;
  bool isNonlinearity() const override { return true; }
};

class SigmoidGradOp : public Op {
public:
  SigmoidGradOp(const SigmoidOp &fwdOp);
  std::unique_ptr<Op> clone() const final;

  const std::vector<GradInOutMapper> &gradInputInfo() const final;
  const std::map<int, int> &gradOutToNonGradIn() const final;
  void setup() final;

  static InIndex getGradInIndex() { return 0; }
  static InIndex getFwdOutInIndex() { return 1; }
  static OutIndex getOutIndex() { return 0; }
};

} // namespace poponnx

#endif
