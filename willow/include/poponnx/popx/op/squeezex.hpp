#ifndef GUARD_NEURALNET_SQUEEZEX_HPP
#define GUARD_NEURALNET_SQUEEZEX_HPP

#include <poponnx/names.hpp>
#include <poponnx/popx/opx.hpp>

namespace poponnx {

class SqueezeOp;
class SqueezeGradOp;

namespace popx {

class SqueezeOpx : public Opx {
public:
  SqueezeOpx(Op *, Devicex *);
  void grow(poplar::program::Sequence &) const final;
};

class SqueezeGradOpx : public Opx {
public:
  SqueezeGradOpx(Op *, Devicex *);
  void grow(poplar::program::Sequence &) const final;
};

} // namespace popx
} // namespace poponnx

#endif
