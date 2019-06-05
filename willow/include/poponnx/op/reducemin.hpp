#ifndef GUARD_NEURALNET_REDUCEMIN_HPP
#define GUARD_NEURALNET_REDUCEMIN_HPP

#include <poponnx/op.hpp>
#include <poponnx/op/reduce.hpp>

namespace poponnx {

class ReduceMinOp : public ReduceOp {
public:
  ReduceMinOp(const OperatorIdentifier &_opid,
              const std::vector<int64_t> &axes,
              const int64_t keepdims,
              const Op::Settings &settings);
  std::unique_ptr<Op> clone() const override;
  std::vector<std::unique_ptr<Op>> getGradOps() final;
};

class ReduceMinGradOp : public ReduceGradOp {
public:
  ReduceMinGradOp(const ReduceMinOp &fwdOp, const Shape &backward_shape);
  std::unique_ptr<Op> clone() const final;
  const std::vector<GradInOutMapper> &gradInputInfo() const final;
  static InIndex getFwdInInIndex() { return 1; }
  static InIndex getFwdOutInIndex() { return 2; }
};

} // namespace poponnx

#endif
