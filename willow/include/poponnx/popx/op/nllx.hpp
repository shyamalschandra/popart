#ifndef GUARD_NEURALNET_NLLX_HPP
#define GUARD_NEURALNET_NLLX_HPP

#include <poponnx/names.hpp>
#include <poponnx/popx/opx.hpp>

namespace willow {

class NllGradOp;
class NllOp;

namespace popx {

class NllOpx : public Opx {
public:
  NllOpx(Op *, Devicex *);
  NllOp *getNllOp() const;
  void grow(poplar::program::Sequence &) const final;
};

class NllGradOpx : public Opx {
public:
  NllGradOpx(Op *, Devicex *);
  NllGradOp *getNllGradOp() const;
};

} // namespace popx
} // namespace willow

#endif