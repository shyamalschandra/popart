#ifndef GUARD_NEURALNET_SOFTMAX_HPP
#define GUARD_NEURALNET_SOFTMAX_HPP

#include <poponnx/ir.hpp>

namespace willow {

class NllLoss;

class SoftmaxOp : public Op {
public:
  SoftmaxOp(const onnx::NodeProto &node, Ir *pir);
  std::unique_ptr<Op> clone() const final;
  std::vector<std::unique_ptr<Op>> getGradOps() final;
  void setup() final;
};

class SoftmaxGradOp : public Op {
public:
  SoftmaxGradOp(SoftmaxOp *);
  const std::vector<GradInOutMapper> &gradInputInfo() const final;
  const std::map<int, int> &gradOutToNonGradIn() const final;
  void setup() final;
  int gradProbsIn() const;
  int actsIn() const;
};

// not a gradient of a single Op, so not inheriting from GradOp
class SoftmaxGradDirectOp : public Op {
public:
  // where Op in this constructor must be a SoftmaxOp
  // where this is created by a merger between the Op
  // and an NllGradOp
  SoftmaxGradDirectOp(Ir *, const NllLoss *);
  std::unique_ptr<Op> clone() const final;
  // this Op has no Grad Ops: throws error if called
  std::vector<std::unique_ptr<Op>> getGradOps() final;
  void setup() final;
  const NllLoss *nlll() const;

private:
  const NllLoss *nllloss_;
};

} // namespace willow

#endif
