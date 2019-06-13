#ifndef GUARD_NEURALNET_ANDX_HPP
#define GUARD_NEURALNET_ANDX_HPP

#include <poponnx/names.hpp>
#include <poponnx/popx/op/elementwisex.hpp>

namespace poponnx {

class AndOp;

namespace popx {

class AndOpx : public BinaryComparisonOpx {
public:
  AndOpx(Op *, Devicex *);
  void grow(poplar::program::Sequence &) const final;
};

} // namespace popx
} // namespace poponnx

#endif
