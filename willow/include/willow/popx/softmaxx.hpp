#ifndef GUARD_NEURALNET_SOFTMAXXXXX_HPP
#define GUARD_NEURALNET_SOFTMAXXXXX_HPP

#include <willow/names.hpp>
#include <willow/popx/opx.hpp>

namespace willow {

class SoftmaxOp;
class SoftmaxGradOp;
class SoftmaxGradDirectOp;

namespace popx {

class SoftmaxOpx : public Opx {
public:
  SoftmaxOpx(Op *, Devicex *);
  SoftmaxOp *getSoftmaxOp() const;
  void grow() const override final;
};

class SoftmaxGradOpx : public Opx {
public:
  SoftmaxGradOpx(Op *, Devicex *);
  SoftmaxGradOp *getSoftmaxGradOp() const;
};

class SoftmaxGradDirectOpx : public Opx {
public:
  SoftmaxGradDirectOpx(Op *, Devicex *);
  SoftmaxGradDirectOp *getSoftmaxGradDirectOp() const;
};

} // namespace popx
} // namespace willow

#endif
