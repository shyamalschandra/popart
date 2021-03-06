// Copyright (c) 2018 Graphcore Ltd. All rights reserved.
#ifndef GUARD_NEURALNET_NOTX_HPP
#define GUARD_NEURALNET_NOTX_HPP

#include <popart/names.hpp>
#include <popart/popx/op/elementwisex.hpp>

namespace popart {

class NotOp;

namespace popx {

class NotOpx : public ElementWiseUnaryOpx {
public:
  NotOpx(Op *, Devicex *);
  void grow(poplar::program::Sequence &) const final;
};

} // namespace popx
} // namespace popart

#endif
