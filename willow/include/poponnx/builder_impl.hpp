#ifndef GUARD_BUILDER_IMPL_H
#define GUARD_BUILDER_IMPL_H

#include <map>
#include <string>
#include <poponnx/builder.hpp>
#include <poponnx/names.hpp>

// The BuilderImpl class has an onnx::ModelProto, so we cannot
// use the forward declarations in names.hpp at this point
#include <onnx/onnx_pb.h>

namespace poponnx {

/**
 * An implementation of a Builder
 */
class BuilderImpl {
public:
  BuilderImpl();

  void configure();

  TensorId addInputTensor(const TensorInfo &tensorInfo);
  TensorId addInitializedInputTensor(const ConstVoidData &initData);

  void addOutputTensor(const TensorId &arg0);

  // Operations requiring only tensor inputs
  TensorId abs(const std::vector<TensorId> &args, const std::string &name);
  TensorId acos(const std::vector<TensorId> &args, const std::string &name);
  TensorId acosh(const std::vector<TensorId> &args, const std::string &name);
  TensorId add(const std::vector<TensorId> &args, const std::string &name);
  TensorId logical_and(const std::vector<TensorId> &args,
                       const std::string &name);
  TensorId asin(const std::vector<TensorId> &args, const std::string &name);
  TensorId asinh(const std::vector<TensorId> &args, const std::string &name);
  TensorId atan(const std::vector<TensorId> &args, const std::string &name);
  TensorId atanh(const std::vector<TensorId> &args, const std::string &name);
  TensorId cast(const std::vector<TensorId> &args, const std::string &name);
  TensorId ceil(const std::vector<TensorId> &args, const std::string &name);
  TensorId cos(const std::vector<TensorId> &args, const std::string &name);
  TensorId cosh(const std::vector<TensorId> &args, const std::string &name);
  TensorId div(const std::vector<TensorId> &args, const std::string &name);
  TensorId elu(const std::vector<TensorId> &args, const std::string &name);
  TensorId equal(const std::vector<TensorId> &args, const std::string &name);
  TensorId exp(const std::vector<TensorId> &args, const std::string &name);
  TensorId floor(const std::vector<TensorId> &args, const std::string &name);
  TensorId greater(const std::vector<TensorId> &args, const std::string &name);
  TensorId identity(const std::vector<TensorId> &args, const std::string &name);
  TensorId less(const std::vector<TensorId> &args, const std::string &name);
  TensorId log(const std::vector<TensorId> &args, const std::string &name);
  TensorId max(const std::vector<TensorId> &args, const std::string &name);
  TensorId mean(const std::vector<TensorId> &args, const std::string &name);
  TensorId min(const std::vector<TensorId> &args, const std::string &name);
  TensorId mul(const std::vector<TensorId> &args, const std::string &name);
  TensorId neg(const std::vector<TensorId> &args, const std::string &name);
  TensorId logical_not(const std::vector<TensorId> &args,
                       const std::string &name);
  TensorId logical_or(const std::vector<TensorId> &args,
                      const std::string &name);
  TensorId pow(const std::vector<TensorId> &args, const std::string &name);
  TensorId reciprocal(const std::vector<TensorId> &args,
                      const std::string &name);
  TensorId relu(const std::vector<TensorId> &args, const std::string &name);
  TensorId sigmoid(const std::vector<TensorId> &args, const std::string &name);
  TensorId sin(const std::vector<TensorId> &args, const std::string &name);
  TensorId sinh(const std::vector<TensorId> &args, const std::string &name);
  TensorId softsign(const std::vector<TensorId> &args, const std::string &name);
  TensorId sqrt(const std::vector<TensorId> &args, const std::string &name);
  TensorId sub(const std::vector<TensorId> &args, const std::string &name);
  TensorId sum(const std::vector<TensorId> &args, const std::string &name);
  TensorId tan(const std::vector<TensorId> &args, const std::string &name);
  TensorId tanh(const std::vector<TensorId> &args, const std::string &name);
  TensorId logical_xor(const std::vector<TensorId> &args,
                       const std::string &name);

  TensorId convolution(const std::vector<TensorId> &args,
                       const std::vector<int64_t> strides,
                       const std::vector<int64_t> padding,
                       const std::vector<int64_t> dilation,
                       int64_t groups,
                       bool cacheOperation,
                       const std::string &name);

  TensorId averagepool(const std::vector<TensorId> &args,
                       const std::vector<int64_t> kernel_shape,
                       const std::vector<int64_t> strides,
                       const std::vector<int64_t> padding,
                       const std::string &name);

  TensorId maxpool(const std::vector<TensorId> &args,
                   const std::vector<int64_t> kernel_shape,
                   const std::vector<int64_t> strides,
                   const std::vector<int64_t> padding,
                   const std::string &name);

  TensorId gemm(const std::vector<TensorId> &args,
                float alpha,
                float beta,
                int64_t transA,
                int64_t transB,
                const std::string &name);

  TensorId pad(const std::vector<TensorId> &args,
               std::string mode,
               const std::vector<int64_t> pads,
               float value,
               const std::string &name);

  TensorId matmul(const std::vector<TensorId> &args, const std::string &name);

  TensorId softmax(const std::vector<TensorId> &args, const std::string &name);

  void addNodeAttribute(const std::string &attributeName,
                        const int64_t &attributeValue,
                        const std::set<TensorId> &nodeOutputNames);

  void addNodeAttribute(const std::string &attributeName,
                        const std::vector<int64_t> &attributeValue,
                        const std::set<TensorId> &nodeOutputNames);

  void addNodeAttribute(const std::string &attributeName,
                        const float &attributeValue,
                        const std::set<TensorId> &nodeOutputNames);

  void addNodeAttribute(const std::string &attributeName,
                        const std::vector<float> &attributeValue,
                        const std::set<TensorId> &nodeOutputNames);

  void addNodeAttribute(const std::string &attributeName,
                        const std::string &attributeValue,
                        const std::set<TensorId> &nodeOutputNames);

  void addNodeAttribute(const std::string &attributeName,
                        const char *attributeValue,
                        const std::set<TensorId> &nodeOutputNames);

  void addNodeAttribute(const std::string &attributeName,
                        const std::vector<std::string> &attributeValue,
                        const std::set<TensorId> &nodeOutputNames);

  void addNodeAttribute(const std::string &attributeName,
                        const bool attributeValue,
                        const std::set<TensorId> &nodeOutputNames);

  bool nodeHasAttribute(const std::string &attributeName,
                        const std::set<TensorId> &nodeOutputNames);

  int64_t getInt64NodeAttribute(const std::string &attributeName,
                                const std::set<TensorId> &nodeOutputNames);

  std::vector<int64_t>
  getInt64VectorNodeAttribute(const std::string &attributeName,
                              const std::set<TensorId> &nodeOutputNames);

  float getFloatNodeAttribute(const std::string &attributeName,
                              const std::set<TensorId> &nodeOutputNames);

  std::vector<float>
  getFloatVectorNodeAttribute(const std::string &attributeName,
                              const std::set<TensorId> &nodeOutputNames);

  std::string getStringNodeAttribute(const std::string &attributeName,
                                     const std::set<TensorId> &nodeOutputNames);

  std::vector<std::string>
  getStringVectorNodeAttribute(const std::string &attributeName,
                               const std::set<TensorId> &nodeOutputNames);

  bool getBoolNodeAttribute(const std::string &attributeName,
                            const std::set<TensorId> &nodeOutputNames);

  void removeNodeAttribute(const std::string &attributeName,
                           const std::set<TensorId> &nodeOutputNames);

  std::vector<std::string>
  getAllNodeAttributeNames(const std::set<TensorId> &nodeOutputNames);

  void loadModelProto(const std::string &modelProtoOrFilename);

  const std::map<std::string, TensorId> getTensorTranslation() const;

  std::string getModelProto() const;

  std::vector<TensorId> getInputTensorIds() const;

  std::vector<TensorId> getOutputTensorIds() const;

  std::vector<int64_t> getTensorShape(const TensorId id);

private:
  TensorId add_simple_op(const std::vector<TensorId> &args,
                         const char *op_type,
                         int arg_count,
                         const std::string &name);

  TensorId add_variadic_op(const std::vector<TensorId> &args,
                           const char *op_type,
                           const std::string &name);

  TensorId getNextId();

  void uniquifyNames(onnx::GraphProto &graph);

  bool isInputTensor(TensorId id) const;

  bool isOutputTensor(TensorId id) const;

  std::string getStrFromTensorIdVec(std::vector<TensorId> v) const;

  int getInputTensorIndex(TensorId id) const;

  int getOutputTensorIndex(TensorId id) const;

  const onnx::ValueInfoProto &getValueInfoProto(TensorId id) const;

  bool
  findNodeProtoByOutputNamesImpl(onnx::NodeProto *&out,
                                 const std::set<TensorId> &nodeOutputNames);

  onnx::NodeProto &
  findNodeProtoByOutputNames(const std::set<TensorId> &nodeOutputNames);

  bool nodeHasAttributeImpl(onnx::AttributeProto *&out,
                            onnx::NodeProto &node,
                            const std::string &attributeName);

  onnx::AttributeProto &
  addNewAttributeToNode(const std::string &attributeName,
                        const std::set<TensorId> &nodeOutputNames);

  onnx::AttributeProto &
  getNodeAttribute(const std::string &attributeName,
                   const std::set<TensorId> &nodeOutputNames);

  uint64_t next_id_ = 0;

  onnx::ModelProto model_;

  std::map<std::string, TensorId> tensorTranslation_;
};

} // namespace poponnx
#endif // GUARD_BUILDER_IMPL_H
