#ifndef GUARD_NEURALNET_CONSTEXPR_HPP
#define GUARD_NEURALNET_CONSTEXPR_HPP

#include <map>
#include <poponnx/attributes.hpp>
#include <poponnx/error.hpp>
#include <poponnx/makeunique.hpp>
#include <poponnx/names.hpp>
#include <poponnx/op.hpp>
#include <poponnx/tensorinfo.hpp>
#include <poponnx/typefunctor.hpp>

namespace poponnx {

// Base class for processing Ops as constant expressions
class ConstExprOp {
public:
  ConstExprOp(Op *);
  virtual ~ConstExprOp() = default;
  // compute the output data of the op
  virtual std::vector<char> compute() = 0;

protected:
  Tensor *inTensor(InIndex) const;
  const TensorInfo &inInfo(InIndex) const;
  const Shape &inShape(InIndex) const;
  const TensorInfo &outInfo0() const;

  template <typename OpFunctor, typename... Args>
  static std::vector<char> callOpFunctor(DataType dtype, Args &&... args);

  // Generic function to safely dynamic_cast the op to the derived type
  template <class OP> OP &getOp() const;

private:
  Op *op;
};

// for every Tensor which is the output of a
// Node in an onnx::Graph, can its value be
// computed just once, on host? If so, we say that
// it is a ConstExprTensor and that its producing
// Node is a ConstExprNode.

// This class exposes functions for determining which
// Tensors are ConstExprTensors, and another for processing
// ConstExprNodes.
class ConstExprUtil {
public:
  // Determine if an op is computable as a ConstExprOp.
  // The rule for determining if an op is computable:
  // For eval and infer, an op is computable if:
  //   1) all the inputs are either Const or Variable Tensors
  // For training, a tensor is computable if:
  //   1) all the inputs are Const Tensors
  static bool isComputable(Op *, Graph &);

  // process a ConstExprOp "op", modfying the Ir pointed to by "ir"
  static void processOp(Op *op, Graph &);

  // Compute all ops possible
  static void foldConstants(Graph &);

private:
  // make the tensor `name` into a constInit tensor
  static void
  makeTensorConstInit(const TensorId name, const void *data, Graph &);
};

// Manager class for ConstExprOp's
class ConstExprOpManager {
public:
  using ConstExprOpFactoryFunc =
      std::function<std::unique_ptr<ConstExprOp>(Op *op)>;

  static void registerConstExprOp(const std::string &type,
                                  ConstExprOpFactoryFunc func);

  static std::unique_ptr<ConstExprOp> createConstExprOp(Op *op);

private:
  ConstExprOpManager();

  // Private not static registration method
  void registerConstExprOpImpl(const std::string &type,
                               ConstExprOpFactoryFunc func);

  // Register the constant ops
  template <typename T> void registerConstOp(const std::string &type);
  void registerConstOps();

  // Singleton
  static ConstExprOpManager &getInstance();

  std::map<std::string, ConstExprOpFactoryFunc> constExprOpMap;
};

// Helper class to register the factor function for ConstExprOp
template <class OP> class ConstExprOpCreator {

  void registerOp(const std::string &type) {
    ConstExprOpManager::registerConstExprOp(
        type, [](Op *op) -> std::unique_ptr<ConstExprOp> {
          return make_unique<OP>(op);
        });
  }

public:
  ConstExprOpCreator(const std::string &type) { registerOp(type); }
};

template <typename OpFunctor, typename... Args>
std::vector<char> ConstExprOp::callOpFunctor(DataType dtype, Args &&... args) {
  return typefunctor::get<OpFunctor, std::vector<char>>(
      dtype, std::forward<Args>(args)...);
}

template <class OP> OP &ConstExprOp::getOp() const {
  OP *d_op = dynamic_cast<OP *>(op);
  if (d_op == nullptr) {
    throw error("Failed to cast to op ({}) derived op ({}), type:{} ",
                typeid(op).name(),
                typeid(d_op).name(),
                op->opid);
  }
  return *d_op;
}

} // namespace poponnx

#endif
