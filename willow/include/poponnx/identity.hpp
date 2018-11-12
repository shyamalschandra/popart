#ifndef GUARD_NEURALNET_IDENTITY_HPP
#define GUARD_NEURALNET_IDENTITY_HPP

#include <poponnx/ir.hpp>

namespace willow {

class IdentityOp : public Op {
public:
  IdentityOp(const OpConstructorBundle &);
  IdentityOp(const onnx::NodeProto &node, Ir *pir);
  std::unique_ptr<Op> clone() const override;
  std::vector<std::unique_ptr<Op>> getGradOps() override final;
  void setup() override final;
};

class IdentityGradOp : public IdentityOp {
public:
  IdentityGradOp(IdentityOp *fwdOp);
  std::unique_ptr<Op> clone() const override final;

  const std::vector<GradInOutMapper> &gradInputInfo() const override final;
  const std::map<int, int> &gradOutToNonGradIn() const override final;
};

} // namespace willow

#endif
