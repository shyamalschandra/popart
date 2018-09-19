#include <neuralnet/logsoftmax.hpp>
#include <neuralnet/tensor.hpp>

namespace neuralnet {

LogSoftmaxOp::LogSoftmaxOp(const onnx::NodeProto &node, Graph *pgraph)
    : Op(node, pgraph) {
  // //std::cout << "in logsoftmax constructor" << std::endl;
}

void LogSoftmaxOp::setup() { output.tensor(0)->info = input.tensor(0)->info; }

} // namespace neuralnet
