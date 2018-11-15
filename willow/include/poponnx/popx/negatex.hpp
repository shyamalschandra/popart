#ifndef GUARD_NEURALNET_NEGATEX_HPP
#define GUARD_NEURALNET_NEGATEX_HPP

#include <poponnx/names.hpp>
#include <poponnx/popx/opx.hpp>

namespace willow {

namespace popx {

class NegateOpx : public Opx {
public:
  NegateOpx(Op *, Devicex *);
  void grow(poplar::program::Sequence &) const final;
};

class NegateGradOpx : public NegateOpx {
public:
  NegateGradOpx(Op *, Devicex *);
};

} // namespace popx
} // namespace willow

#endif
