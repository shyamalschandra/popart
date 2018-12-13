#ifndef GUARD_NEURALNET_TRANSPOSEX_HPP
#define GUARD_NEURALNET_TRANSPOSEX_HPP

#include <poponnx/names.hpp>
#include <poponnx/popx/opx.hpp>

namespace poponnx {

namespace popx {

class TransposeOpx : public Opx {
public:
  TransposeOpx(Op *, Devicex *);
  void grow(poplar::program::Sequence &) const final;
};

class TransposeGradOpx : public TransposeOpx {
public:
  TransposeGradOpx(Op *, Devicex *);
};

} // namespace popx
} // namespace poponnx

#endif