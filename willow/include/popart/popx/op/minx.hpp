// Copyright (c) 2018 Graphcore Ltd. All rights reserved.
#ifndef GUARD_NEURALNET_MINX_HPP
#define GUARD_NEURALNET_MINX_HPP

#include <popart/names.hpp>
#include <popart/popx/op/elementwisex.hpp>

namespace popart {

namespace popx {

// Refactor needed, see T7199
class MinOpx : public ElementWiseUnaryOpx {
public:
  MinOpx(Op *, Devicex *);
  void grow(poplar::program::Sequence &) const final;
};

class MinArgGradOpx : public Opx {
public:
  MinArgGradOpx(Op *, Devicex *);
  void grow(poplar::program::Sequence &) const final;
};

} // namespace popx
} // namespace popart

#endif
