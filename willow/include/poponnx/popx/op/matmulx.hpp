#ifndef GUARD_NEURALNET_MATMULX_HPP
#define GUARD_NEURALNET_MATMULX_HPP

#include <poponnx/names.hpp>
#include <poponnx/popx/opx.hpp>

namespace willow {

class MatMulOp;
class MatMulLhsGradOp;
class MatMulRhsGradOp;

namespace popx {

class MatMulOpx : public Opx {
public:
  MatMulOpx(Op *, Devicex *);
  ~MatMulOpx() override = default;

  poplar::Tensor createInput(int index) const final;
  bool canCreateInput(int index) const final;
  bool createsEquiv(int, Opx *, int) const final;
  std::vector<TensorId> mustExistBeforeCreate(int index0) const final;

  MatMulOp *getMatMulOp() const;
  void grow(poplar::program::Sequence &) const final;

  static std::vector<std::size_t> onnxShapeToPoplar(const Shape &shape);

private:
  // The ONNX tensor shape
  std::vector<std::size_t> getLhsInputShape() const;
  std::vector<std::size_t> getRhsInputShape() const;
  std::vector<std::size_t> getOutputShape() const;

  // The Poplar allocation shape
  // Must be a 3D tensor where the first dimension is interpreted as being the
  // stack size.
  std::vector<std::size_t> getLhsInputAllocShape() const;
  std::vector<std::size_t> getRhsInputAllocShape() const;

  // The Poplar allocation shape of the opposite operand, with a matching stack
  // size.
  std::vector<std::size_t> getLhsInputCoallocShape() const;
  std::vector<std::size_t> getRhsInputCoallocShape() const;
};

class MatMulLhsGradOpx : public Opx {
public:
  MatMulLhsGradOpx(Op *, Devicex *);
  ~MatMulLhsGradOpx() override = default;

  MatMulLhsGradOp *getMatMulLhsGradOp() const;
  void grow(poplar::program::Sequence &) const final;

private:
  // the Poplar tensor shape
  // The shape of the grad op's gradient input
  std::vector<std::size_t> getGradInputShape() const;
  // The shape of the grad op's rhs input
  std::vector<std::size_t> getRhsInputShape() const;
  // The shape of the grad op's output
  std::vector<std::size_t> getOutputShape() const;

  // The ONNX shape to broadcast into to be Poplar reshape compatible
  Shape getGradInputBroadcastShape() const;
  Shape getRhsInputBroadcastShape() const;

  // The Poplar reduction axes
  std::vector<std::size_t> getOutputReductionAxes() const;
};

class MatMulRhsGradOpx : public Opx {
public:
  MatMulRhsGradOpx(Op *, Devicex *);
  ~MatMulRhsGradOpx() override = default;

  MatMulRhsGradOp *getMatMulRhsGradOp() const;
  void grow(poplar::program::Sequence &) const final;

private:
  // the Poplar tensor shape
  // The shape of the grad op's gradient input
  std::vector<std::size_t> getLhsInputShape() const;
  // The shape of the grad op's rhs input
  std::vector<std::size_t> getGradInputShape() const;
  // The shape of the grad op's output
  std::vector<std::size_t> getOutputShape() const;

  // The ONNX shape to broadcast into to be Poplar reshape compatible
  Shape getLhsInputBroadcastShape() const;
  Shape getGradInputBroadcastShape() const;

  // The Poplar reduction axes
  std::vector<std::size_t> getOutputReductionAxes() const;
};

} // namespace popx
} // namespace willow

#endif