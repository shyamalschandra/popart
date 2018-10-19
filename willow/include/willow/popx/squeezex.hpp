#ifndef GUARD_NEURALNET_SQUEEZEX_HPP
#define GUARD_NEURALNET_SQUEEZEX_HPP

#include <willow/names.hpp>
#include <willow/popx/opx.hpp>

namespace willow {

class SqueezeOp;

namespace popx {

class SqueezeOpx : public Opx {
public:
  SqueezeOpx(Op *);
  SqueezeOp *getSqueezeOp() const;
};

} // namespace popx
} // namespace willow

#endif