#ifndef GUARD_NEURALNET_SUM_HPP
#define GUARD_NEURALNET_SUM_HPP

#include <neuralnet/graph.hpp>

namespace neuralnet {

class SumOp : public Op {
public:
  SumOp(const OpConstructorBundle &);
  virtual void setup() override final;
};
} // namespace neuralnet

#endif
