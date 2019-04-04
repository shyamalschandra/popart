#ifndef GUARD_NEURALNET_TANH_HPP
#define GUARD_NEURALNET_TANH_HPP

#include <poponnx/op.hpp>

namespace poponnx {

class TanhOp : public Op {
public:
  TanhOp(const OperatorIdentifier &_opid, const Op::Settings &settings_);
  std::unique_ptr<Op> clone() const override;
  std::vector<std::unique_ptr<Op>> getGradOps() final;
  void setup() final;

  static InIndex getInIndex() { return 0; }
  static OutIndex getOutIndex() { return 0; }

  bool isNonlinearity() const override { return true; }
};

class TanhGradOp : public Op {
public:
  TanhGradOp(const TanhOp &fwdOp);
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
