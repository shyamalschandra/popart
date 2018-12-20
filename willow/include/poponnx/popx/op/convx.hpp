#ifndef GUARD_NEURALNET_CONVX_HPP
#define GUARD_NEURALNET_CONVX_HPP

#include <poponnx/names.hpp>
#include <poponnx/popx/enigma.hpp>
#include <poponnx/popx/opx.hpp>

#include <poplin/Convolution.hpp>

namespace poponnx {

class ConvOp;
class ConvWeightsGradOp;
class ConvDataGradOp;

namespace popx {

poplin::ConvParams getFwdConvParams(const ConvOp *cOp);
poplin::ConvParams getDataGradParams(const ConvDataGradOp *convDataGradOp);

class ConvOpx : public Opx {
public:
  ConvOpx(Op *, Devicex *);
  poplar::Tensor createInput(InIndex index) const final;
  bool canCreateInput(InIndex index) const final;
  bool createsEquiv(int, Opx *, int) const final;
  std::vector<TensorId> mustExistBeforeCreate(InIndex index0) const final;
  const poplin::ConvParams &getParams() const;
  void grow(poplar::program::Sequence &) const final;

private:
  poplin::ConvParams fwdParams;
};

class ConvDataGradOpx : public Opx {
public:
  ConvDataGradOpx(Op *, Devicex *);
  void grow(poplar::program::Sequence &) const final;
  const poplin::ConvParams &getParams() const;

private:
  poplin::ConvParams dataGradParams;
};

class ConvWeightsGradOpx : public Opx {
public:
  ConvWeightsGradOpx(Op *, Devicex *);
  void grow(poplar::program::Sequence &) const final;
};

} // namespace popx
} // namespace poponnx

#endif
