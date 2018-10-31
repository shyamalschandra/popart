#ifndef GUARD_NEURALNET_WILLOWNET_HPP
#define GUARD_NEURALNET_WILLOWNET_HPP

#include <willow/names.hpp>

namespace willow {

class WillowNet {
public:
  WillowNet(std::string fnOnnxModel,
            const EarlyInfo &,
            const DataFlow &,
            const std::vector<Loss *> &,
            const Optimizer *,
            const std::vector<std::string> &cTens,
            std::string logdir_,
            const std::vector<std::string> &patternNames);

  ~WillowNet();

  // update the optimizer. Note that the optimizer passed in
  // must be compatible with that in the constructor
  // Must call optimizerToDevice to take effect.
  void updateOptimizer(const Optimizer *);

  // see exampledriver.cpp in poponnx for an idea of what should be done here
  void setDevice(std::string x);

  // for IPUs, this will be creating the poplar::Graph,
  // poplar::Engine setting up poplar::Streams etc
  void prepareDevice();

  // write to device, from an onnx model loaded from directory,
  void weightsFromHost();

  // write whatever optimizer tensors (learning rates,
  // momentum, initial momentum tensors (zero)) there are to device
  void optimizerFromHost();

  // take training steps, number of steps specified in DataFlow
  // input data from address in stepIO.in
  // output data to addresses in stepIO.out
  // For Poplar, this will involve reading and writing
  // Poplar::Stream host addresses <--> these addresses.
  void step(const StepIO &stepIO);

  // write current model to ONNX file
  void modelToHost(std::string fn);

  // get the TensorInfo on a Tensor
  TensorInfo getInfo(TensorId) const;

private:
  // abstraction of the computation
  std::unique_ptr<Ir> pir;
  std::unique_ptr<Device> device_;
};
} // namespace willow

#endif
