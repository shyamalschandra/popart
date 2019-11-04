#include <algorithm>
#include <array>
#include <boost/test/unit_test.hpp>
#include <iostream>
#include <map>
#include <random>
#include <tuple>
#include <vector>

#define protected public
#include <popart/builder.hpp>
#include <popart/dataflow.hpp>
#include <popart/devicemanager.hpp>
#include <popart/error.hpp>
#include <popart/filereader.hpp>
#include <popart/inputshapeinfo.hpp>
#include <popart/ndarraywrapper.hpp>
#include <popart/op/ipucopy.hpp>
#include <popart/op/l1.hpp>
#include <popart/op/restore.hpp>
#include <popart/op/sgd0varupdate.hpp>
#include <popart/op/stash.hpp>
#include <popart/optimizer.hpp>
#include <popart/session.hpp>
#include <popart/tensorinfo.hpp>
#include <popart/tensornames.hpp>
#undef protected

// TODO T12595 : put the implemenations below into a .cpp file

namespace {
// names of 2 weights used in model
constexpr const char *w0name = "__w0__";
constexpr const char *w1name = "__w1__";
int64_t replicationFactor    = 2;
int64_t accumulationFactor   = 5;
// TODO T10881 : cleaner solution to acquisition of IPU failure
std::array<float, 2> acquisitionFailure{-99.0f, -99.0f};
} // namespace

float getAbsDiff(float expected, float observed) {
  float absv = std::abs(expected - observed);
  std::cout << "Expected=" << expected << ", observed=" << observed
            << " with absolute difference=" << absv << std::endl;
  return absv;
}

// The pytorch update equations:
//
// (1) g = g + wd * w
// (2) v = v * mm + (1 - dp) * g
// (3) w = w - lr * v
//
void pytorchUpdate(float &w,
                   float &g,
                   float &v,
                   float wd,
                   float mm,
                   float dp,
                   float lr) {

  g = +1.0f; // from the model below
  g += wd * w;
  v *= mm;
  v += (1.0f - dp) * g;
  w -= lr * v;
}

void laggedPytorchUpdate(float &w,
                         float &g,
                         float &v,
                         float wd,
                         float mm,
                         float dp,
                         float lr,
                         int64_t replFactor = 1,
                         int64_t acclFactor = 1) {

  g = +1.0f *
      static_cast<float>(replFactor * acclFactor); // from the model below
  v = v + (1.0f - dp) * g;
  w = w - lr * v;
  v = v * mm + (1.0f - dp) * wd * w;
}

void laggedPytorchUpdateWithScaling(float &w,
                                    float &g,
                                    float &v,
                                    float wd,
                                    float mm,
                                    float dp,
                                    float lr,
                                    float vs,
                                    float ls) {

  g = +1.0f * ls;
  v = v + vs * (1.0f - dp) * g / ls;
  w = w - lr * v / vs;
  v = v * mm + vs * (1.0f - dp) * wd * w;
}

std::array<float, 2>
getResults(const popart::SGD &opt0, // initial Optimizer
           const popart::SGD &opt1, // Optimizer to switch to
           const popart::SGD &opt2, // Last Optimizer
           bool gradAccl,
           bool graphRepl) {

  using namespace popart;
  // Model
  // ----
  //
  // loss = l1_loss((input + w0) + w1)
  //
  // where input is small positive and w0, w1 are large positive
  //

  auto builder = Builder::create();
  auto aiOnnx  = builder->aiOnnxOpset9();

  int64_t sampleDim            = 1;
  int64_t samplesPerMicroBatch = 1;
  int64_t samplesPerBatch      = (graphRepl ? replicationFactor : 1) *
                            (gradAccl ? accumulationFactor : 1) *
                            samplesPerMicroBatch;
  int64_t batchesPerStep = 1;

  std::vector<int64_t> microBatchShape{samplesPerMicroBatch, sampleDim};
  TensorInfo microBatchInfo{"FLOAT", microBatchShape};

  std::vector<int64_t> batchShape{samplesPerBatch, sampleDim};
  TensorInfo batchInfo{"FLOAT", microBatchShape};

  std::vector<int64_t> sampleShape{sampleDim};
  TensorInfo sampleInfo{"FLOAT", sampleShape};

  std::vector<int64_t> stepDataShape{
      batchesPerStep, samplesPerBatch, sampleDim};
  TensorInfo stepDataInfo{"FLOAT", stepDataShape};

  auto input0 = builder->addInputTensor(microBatchInfo, "0tupni");

  WeightsIO weightsRead;

  std::vector<float> weight0(sampleDim, 100.0f);
  std::vector<float> rb0(sampleDim, -777.0f);
  ConstVoidData cvd0({weight0.data(), sampleInfo});
  auto w0Id = builder->addInitializedInputTensor(cvd0, w0name);
  assert(w0Id == w0name);
  weightsRead.insert(w0Id, {rb0.data(), sampleInfo});

  std::vector<float> weight1(sampleDim, 200.0f);
  std::vector<float> rb1(sampleDim, -777.0f);
  ConstVoidData cvd1({weight1.data(), sampleInfo});
  auto w1Id = builder->addInitializedInputTensor(cvd1, w1name);
  assert(w1Id == w1name);
  weightsRead.insert(w1Id, {rb1.data(), sampleInfo});

  auto add0 = aiOnnx.add({w0Id, input0});
  auto add1 = aiOnnx.add({w1Id, add0});
  builder->addOutputTensor(add1);

  auto proto    = builder->getModelProto();
  auto dataFlow = DataFlow(batchesPerStep, {});

  float lambda = 1.0;
  auto loss    = std::unique_ptr<Loss>(
      new L1Loss(add1, "l1LossVal", lambda, ReductionType::SUM));

  SessionOptions userOptions;
  std::map<std::string, std::string> deviceOpts{{"numIPUs", "1"}};

  userOptions.enableOutlining = false;

  if (gradAccl) {
    userOptions.enableGradientAccumulation = true;
    userOptions.accumulationFactor         = accumulationFactor;
  }

  if (graphRepl) {
    deviceOpts["numIPUs"]              = std::to_string(replicationFactor);
    userOptions.enableReplicatedGraphs = true;
    userOptions.replicatedGraphCount   = replicationFactor;
  }

  std::shared_ptr<DeviceInfo> device;
  if (!graphRepl) {
    device =
        DeviceManager::createDeviceManager().createIpuModelDevice(deviceOpts);
  } else {

    // TODO : cleaner acquisition failure solution T10881
    bool errorIfFailToAcquire = false;

    auto devices =
        popart::DeviceManager::createDeviceManager().enumerateDevices();
    if (devices.size() == 0) {
      if (errorIfFailToAcquire) {
        throw error("Failed to enumerate any devices in get_results.hpp");
      } else {
        return acquisitionFailure;
      }
    }

    device = DeviceManager::createDeviceManager().acquireAvailableDevice(
        replicationFactor);

    if (!device) {
      if (errorIfFailToAcquire) {
        throw error("Failed to acquire device in get_results.hpp");
      } else {
        return acquisitionFailure;
      }
    }
  }

  auto session = popart::TrainingSession::createFromOnnxModel(
      proto,
      dataFlow,
      {loss.get()},
      opt0, // construct with opt0, will switch to opt1, opt2 later
      device,
      InputShapeInfo(),
      userOptions,
      popart::Patterns(PatternsLevel::DEFAULT));

  session->prepareDevice();
  std::vector<float> v_input_x(stepDataInfo.nelms(), 3.1415);

  popart::NDArrayWrapper<float> input_x_wrapper(v_input_x.data(), stepDataInfo);

  std::map<popart::TensorId, popart::IArray &> inputs = {
      {input0, input_x_wrapper}};

  popart::StepIO stepio(inputs, {});

  session->weightsFromHost();

  // run 1 with opt0
  session->optimizerFromHost();
  session->run(stepio);

  // run 2 with opt1
  session->updateOptimizer(&opt1);
  session->optimizerFromHost();
  session->run(stepio);

  // run 3 with opt2
  session->updateOptimizer(&opt2);
  session->optimizerFromHost();
  session->run(stepio);

  // read final weights back
  session->weightsToHost();
  session->readWeights(weightsRead);

  return std::array<float, 2>{rb0[0], rb1[0]};
}