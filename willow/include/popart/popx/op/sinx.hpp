#ifndef GUARD_NEURALNET_SINX_HPP
#define GUARD_NEURALNET_SINX_HPP

#include <popart/names.hpp>
#include <popart/popx/op/elementwisex.hpp>

namespace popart {

namespace popx {

class SinOpx : public ElementWiseUnaryOpx {
public:
  SinOpx(Op *, Devicex *);
  void grow(poplar::program::Sequence &) const final;
};

} // namespace popx
} // namespace popart

#endif