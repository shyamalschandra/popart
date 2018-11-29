#ifndef GUARD_NEURALNET_L1_HPP
#define GUARD_NEURALNET_L1_HPP

#include <poponnx/ir.hpp>
#include <poponnx/op/loss.hpp>

namespace poponnx {

class L1Loss : public Loss {
public:
  // where lambda*|"input"|_1 = "output" (so output has rank 0)
  L1Loss(TensorId input, TensorId output, float lambda);
  // There are no tensors streamed into this loss layer (unlike NLL for
  // example which has a label streamed in)
  std::vector<TensorId> getStreamTensorNames() const final;
  std::unique_ptr<Op> getOp(Ir *) const final;
  std::string op_type() const final;
  TensorId getInputId() const;
  float getLambda() const;
  std::unique_ptr<Loss> clone() const final {
    return std::unique_ptr<Loss>(new L1Loss(*this));
  }

private:
  float lambda;
};

class L1Op : public LossOp {
public:
  L1Op(const OpConstructorBundle &, const L1Loss *l1loss);
  std::unique_ptr<Op> clone() const final;
  std::vector<std::unique_ptr<Op>> getGradOps() final;
  void setup() final;
  const L1Loss *l1l() const;

private:
  const L1Loss *l1loss_;
};

class L1GradOp : public Op {

public:
  L1GradOp(L1Op *);
  const std::vector<GradInOutMapper> &gradInputInfo() const final;
  const std::map<int, int> &gradOutToNonGradIn() const final;
  void setup() final;
  const L1Loss *l1l() const;

private:
  const L1Loss *l1loss_;
};

} // namespace poponnx

#endif
