#include <algorithm>
#include <poponnx/makeunique.hpp>
#include <poponnx/op/subsample.hpp>
#include <poponnx/tensor.hpp>
#include <poponnx/tensorindex.hpp>
#include <poponnx/util.hpp>

namespace poponnx {

SubsampleOp::SubsampleOp(const OpConstructorBundle &bundle) : Op(bundle) {}

SubsampleOp::SubsampleOp(const onnx::NodeProto &node, Ir *_pir)
    : Op(node, _pir) {}

std::unique_ptr<Op> SubsampleOp::clone() const {
  return make_unique<SubsampleOp>(*this);
}

std::vector<std::unique_ptr<Op>> SubsampleOp::getGradOps() {
  std::vector<std::unique_ptr<Op>> upops;
  upops.emplace_back(make_unique<SubsampleGradOp>(this));
  return upops;
}

// We are subsampling the tensor
void SubsampleOp::setup() {

  // Get the stride attribute
  nAtts.set(strides, "strides");

  // Verify that a stride of 0 has not be used
  for (int i = 0; i < strides.size(); ++i) {
    if (strides[i] == 0)
      throw error("Strides invalid. 0 stride at index {}", i);
  }

  // Type will be the same
  DataType outType = inInfo(getInIndex()).dataType();

  // Now calculate the shape of the output tensor.
  // The rank will be the same, but the value of the dimensions will be
  // different
  Shape outShape;
  Shape _inShape = inShape(getInIndex());
  for (int i = 0; i < strides.size(); ++i) {
    // We have already checked for a stride of 0, so we do not have to worry
    // about divide by 0 Poplar rounds up if stride is not an a factor of the
    // dimension
    outShape.push_back((_inShape[i] + strides[i] - 1) / strides[i]);
  }

  outInfo(getOutIndex()).set(outType, outShape);
}

std::vector<uint32_t> SubsampleOp::strides_u32() const {
  return vXtoY<int64_t, uint32_t>(strides);
}

bool SubsampleOp::strideSizeOne() const {
  return std::all_of(
      strides.cbegin(), strides.cend(), [](int64_t p) { return p == 1; });
}

SubsampleGradOp::SubsampleGradOp(SubsampleOp *_fwdOp)
    : Op({OpType::SUBSAMPLEGRAD, _fwdOp->pir, {}}), fwdOp(_fwdOp),
      fwdOpInfo(_fwdOp->inInfo(0)) {}

std::unique_ptr<Op> SubsampleGradOp::clone() const {
  return make_unique<SubsampleGradOp>(*this);
}

void SubsampleGradOp::setup() { output->tensor(0)->info = fwdOpInfo; }

const std::vector<GradInOutMapper> &SubsampleGradOp::gradInputInfo() const {
  static const std::vector<GradInOutMapper> inInfo = {
      {getInIndex(), SubsampleOp::getOutIndex(), GradOpInType::GRADOUT}};

  return inInfo;
}

const std::map<int, int> &SubsampleGradOp::gradOutToNonGradIn() const {
  static const std::map<int, int> outInfo = {
      {getOutIndex(), SubsampleOp::getInIndex()}};

  return outInfo;
}

} // namespace poponnx