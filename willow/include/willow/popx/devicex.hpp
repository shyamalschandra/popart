#ifndef GUARD_NEURALNET_POPDEVICE_HPP
#define GUARD_NEURALNET_POPDEVICE_HPP

#pragma clang diagnostic push // start ignoring warnings
#pragma clang diagnostic ignored "-Weverything"
#include <poplar/DeviceManager.hpp>
#include <poplar/Engine.hpp>
#include <poplar/Graph.hpp>
#include <poplar/IPUModel.hpp>
#include <poplin/Convolution.hpp>
#include <poputil/TileMapping.hpp>
#pragma clang diagnostic pop // stop ignoring warnings

#include <willow/device.hpp>
#include <willow/popx/enigma.hpp>
#include <willow/pritask.hpp>

namespace willow {
namespace popx {

using PopStreamId = std::string;

class Opx;

class PopPrograms {

public:
  enum ProgramIndex {
    WEIGHTSFROMHOST = 0,
    OPTIMIZERFROMHOST,
    STEP,
    WEIGHTSTOHOST,
    N // The number of programs
  };

  poplar::program::Sequence &weightsFromHost();
  poplar::program::Sequence &optimizerFromHost();
  poplar::program::Sequence &step();
  poplar::program::Sequence &weightsToHost();
  std::vector<poplar::program::Program> progs();

private:
  std::array<poplar::program::Sequence, ProgramIndex::N> seqs;
};

poplar::Type popType(const TensorInfo &);

// A bundle class for an int and an Opx.
class OpxAndInIndex {
public:
  OpxAndInIndex(int, Opx *);
  OpxAndInIndex() = default;
  int index;
  Opx *opx;
};

class Devicex : public willow::Device {

public:
  Devicex(const Ir *);
  virtual void prepare() override final;
  virtual void weightsFromHost() override final;
  virtual void optimizerFromHost() override final;
  virtual void step(const StepIO &) override final;
  Opx *getOpx(OpId);
  poplar::Graph &graph();

  // enigma has a PlanningCache for matmul and conv
  poplin::PlanningCache convCache;
  poplin::PlanningCache matmulCache;

  // completed in Devicex constructor.
  enigma::ConvOptions fwdConvOptions, bwdConvOptions, wuConvOptions;
  poplar::OptionFlags engineOptions;

  // return the name of the task which creates a poplar::Tensor
  // This function is mostly string manipulation
  TaskId taskWhichCreates(TensorId) const;

  const poplar::Tensor & getTensor(TensorId);
  void insert(TensorId, const poplar::Tensor &);

  PopPrograms progs;

private:
  std::unique_ptr<poplar::Graph> pGraph{nullptr};
  std::unique_ptr<poplar::Engine> pEngine{nullptr};
  std::unique_ptr<poplar::Target> pTarget{nullptr};
  poplar::Device popDevice;


  // Task to create a poplar::Tensor from nothing, choosing
  // the correct create call (createWeights, addLinearly, etc)
  PriTask initTensorTask(Tensor *tensor);
  TaskId initTensorTaskId(TensorId) const;

  // Task to create a poplar::Stream to write to poplar::Tensor
  // C++ Note: if a lambda function which modifies `this' is created
  // it must be const w.r.t this, even if it not run
  PriTask streamFromHostTask(Tensor *tensor);
  TaskId streamFromHostTaskId(TensorId) const;

  // Task to create a poplar::Stream to write from poplar::Tensor
  PriTask streamToHostTask(Tensor *tensor);
  TaskId streamToHostTaskId(TensorId) const;

  // Task to append a Copy from poplar::Stream to poplar::Tensor
  PriTask fromHostTask(Tensor *tensor, poplar::program::Sequence &) const;
  TaskId fromHostTaskId(TensorId) const;
  
  PriTask opTask(Op *, double  priority);
  TaskId opTaskId(Op *) const;

  // The ID of the poplar::Stream host->device for poplar::Tensor
  PopStreamId h2dId(TensorId) const;
  // and for device->host
  PopStreamId d2hId(TensorId) const;

  std::unique_ptr<Opx> createOpx(Op *);

  // 1-to-1 mapping between Ops and Opxs
  std::map<OpId, std::unique_ptr<Opx>> opxs;
  std::map<TensorId, poplar::Tensor> popTensors;

  // the poplar::Streams for poplar::Tensors,
  // from host to device:
  std::map<TensorId, poplar::DataStream> fromHostStreams;
  // and from device to host:
  std::map<TensorId, poplar::DataStream> toHostStreams;

  std::map<TensorId, std::vector<char>> h2dBuffers;
  std::map<TensorId, std::vector<char>> d2hBuffers;

  // copy a step tensor from user provided src, to allocated memory dst
  void
  copyToStreamHostAddr(void *dst,       // destination of copy (a step tensor)
                       const void *src, // source of copy
                       const TensorInfo &dstInfo, // the info for dst
                       const TensorInfo &srcInfo, // user provided info for src
                       TensorId id);
};

} // namespace popx
} // namespace willow

#endif
