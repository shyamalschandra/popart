#ifndef GUARD_NEURALNET_PADGRAD_HPP
#define GUARD_NEURALNET_PADGRAD_HPP

#include <poponnx/op/slice.hpp>

// The PadGradOp has been moved out of the pad.hpp due to a
// circular dependency between PadGradOp inheriting from SliceOp
// and SliceGradOp inheriting from PadOp

namespace poponnx {

class PadGradOp : public SliceOp {
public:
  PadGradOp(const PadOp &fwdOp);
  std::unique_ptr<Op> clone() const final;

  const std::vector<GradInOutMapper> &gradInputInfo() const final;
  const std::map<int, int> &gradOutToNonGradIn() const final;

  float getSubgraphValue() const final { return 0.1f; }

private:
  static std::vector<int64_t> calculateStarts(const PadOp &);
  static std::vector<int64_t> calculateEnds(const PadOp &);
  static std::vector<int64_t> calculateAxes(const PadOp &);
};

} // namespace poponnx

#endif
