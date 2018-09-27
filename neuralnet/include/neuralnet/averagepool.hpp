#ifndef GUARD_NEURALNET_AVERAGEPOOL_HPP
#define GUARD_NEURALNET_AVERAGEPOOL_HPP

#include <neuralnet/graph.hpp>
#include <neuralnet/names.hpp>
#include <neuralnet/receptive.hpp>

namespace neuralnet {

class Tensor;

class AveragePoolOp : public HasReceptiveFieldOp {
public:
  AveragePoolOp(const onnx::NodeProto &node, Graph *pgraph);
  virtual std::vector<std::unique_ptr<Op>> getGradOps() override final;

private:
  virtual void setup0() override final;
  virtual void setSpatial() override final;
  int64_t getNOutChans() const override final;
};

class AveragePoolGradOp : public GradOp {

public:
  AveragePoolGradOp(AveragePoolOp *);
  virtual Op *getNonGradOp() const override final;
  virtual const std::vector<GradInOutMapper> &
  gradInputInfo() const override final;
  virtual const std::map<int, int> &gradOutToNonGradIn() const override final;
  void setup() override final;

private:
  std::vector<GradInOutMapper> createAveragePoolGradInfo() const;
  std::map<int, int> createAveragePoolGradOutToIn() const;
  AveragePoolOp *averagePoolOp;
};

} // namespace neuralnet

#endif