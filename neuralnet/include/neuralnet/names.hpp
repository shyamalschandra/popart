#ifndef GUARD_NEURALNET_NAMES_HPP
#define GUARD_NEURALNET_NAMES_HPP

#pragma clang diagnostic push // start ignoring warnings
#pragma clang diagnostic ignored "-Weverything"
#include <onnx/onnx.pb.h>
#pragma clang diagnostic pop // stop ignoring warnings

#include <ostream>
#include <string>

namespace neuralnet {

// class TensorId{
//
// public:
//  TensorId(std::string x):name(x) {}
//  const std::string & str() const { return name;}
//
// private:
//    std::string name;
//
//};
//
// bool operator<(const TensorId & a, const TensorId & b){
//  return a.str() < b.str();
//}
//
// std::ostream &operator<<(std::ostream &os, const TensorId &id) {
//  os << id.str();
//  return os;
//}
//

using TensorId = std::string;

using OpId = int;
// using TensorId = std::string;

// The position at which a Tensor is consumed by an Op
using InIndex = int;

// The position at which a Tensor is output by an Op
using OutIndex = int;

using Node = onnx::NodeProto;

using onnxAttPtr = decltype(&onnx::NodeProto().attribute(0));

using OnnxTensors = std::map<TensorId, onnx::TensorProto>;

using OnnxTensorPtrs = std::map<TensorId, const onnx::TensorProto *>;

using DataType = decltype(onnx::TensorProto().data_type());

using TP = onnx::TensorProto;

} // namespace neuralnet

#endif