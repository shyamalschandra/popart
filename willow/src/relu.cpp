#include <poponnx/relu.hpp>
#include <poponnx/tensor.hpp>

namespace willow {

std::unique_ptr<Op> ReluOp::clone() const {
  return std::unique_ptr<Op>(new ReluOp(*this));
}

ReluOp::ReluOp(const onnx::NodeProto &node, Ir *pir) : Op(node, pir) {}

std::vector<std::unique_ptr<Op>> ReluOp::getGradOps() {
  std::vector<std::unique_ptr<Op>> upops;
  upops.emplace_back(std::unique_ptr<Op>(new ReluGradOp(this)));
  return upops;
}

void ReluOp::setup() { output.tensor(0)->info = input.tensor(0)->info; }

void ReluGradOp::setup() { output.tensor(0)->info = input.tensor(0)->info; }

ReluGradOp::ReluGradOp(ReluOp *op_)
    : Op({"ReluGrad", op_->pir, {}, getWillowDomain()}) {}

const std::vector<GradInOutMapper> &ReluGradOp::gradInputInfo() const {
  static const std::vector<GradInOutMapper> inInfo = createReluGradInfo();
  return inInfo;
}

std::map<int, int> ReluGradOp::createReluGradOutToIn() const {
  // the grad-op output at index 0 corresponds
  // to the non-grad-op's input at index 0
  return {{0, 0}};
}

const std::map<int, int> &ReluGradOp::gradOutToNonGradIn() const {
  static const std::map<int, int> outInfo = createReluGradOutToIn();
  return outInfo;
}

int ReluGradOp::getReludIn() const { return 1; }

int ReluGradOp::getGradReludIn() const { return 0; }

std::vector<GradInOutMapper> ReluGradOp::createReluGradInfo() const {
  // input at index getGradReludIn() (=0) : gradient of output of relu
  // input at index getReludIn() (=1)     : output of relu
  // can we do better sometimes with in-placing?
  // The 0's below : As the there is only 1 output of Relu, it
  // is output at index 0.
  return {{getGradReludIn(), 0, GradOpInType::GRADOUT},
          {getReludIn(), 0, GradOpInType::OUT}};
}

} // namespace willow
