#include <poponnx/error.hpp>
#include <poponnx/ir.hpp>
#include <poponnx/op/groupnorm.hpp>
#include <poponnx/popx/devicex.hpp>
#include <poponnx/popx/op/groupnormx.hpp>
#include <poponnx/popx/opxmanager.hpp>

#include <poplar/Tensor.hpp>
#include <poplin/Norms.hpp>
#include <popnn/GroupNorm.hpp>
#include <popops/ElementWise.hpp>
#include <popops/Expr.hpp>

namespace poplar {
using Shape = std::vector<std::size_t>;
}

namespace pe = popops::expr;

namespace poponnx {
namespace popx {

GroupNormOpx::GroupNormOpx(Op *op, Devicex *devicex) : NormOpx(op, devicex) {
  verifyOp<GroupNormOp>(op, {Onnx::CustomOperators::GroupNormalization_1});
}

void GroupNormOpx::grow(poplar::program::Sequence &prog) const {

  auto op = getOp<GroupNormOp>();

  // Get the attributes
  float epsilon      = op.getEpsilon();
  int64_t num_groups = op.getNumGroups();
  // int64_t num_channels = op.getNumChannels();

  // Get the inputs
  auto input = getInTensor(GroupNormOp::getXInIndex());
  auto scale = getInTensor(GroupNormOp::getScaleInIndex());
  auto b     = getInTensor(GroupNormOp::getBInIndex());

  // Convert input shape to poplar rules
  poplar::Tensor inputP;
  poplar::Shape nonBroadcastDims;
  std::tie(inputP, nonBroadcastDims) = convertOnnxInputToPoplarInput(input);

  // Calculate the mean and the inverse standard deviation
  poplar::Tensor mean;
  poplar::Tensor invStdDev;
  std::tie(mean, invStdDev) =
      popnn::gn::groupNormStatistics(graph(),
                                     inputP,
                                     epsilon,
                                     prog,
                                     static_cast<unsigned int>(num_groups),
                                     false);

  // Calculate the normalization
  auto result = popnn::gn::groupNormalise(
      graph(), input, scale, b, mean, invStdDev, prog, idStr() + "/groupNorm");

  // Convert the output back into the input format
  poplar::Tensor y =
      convertPoplarOutputToOnnxOutput(result.first, nonBroadcastDims);

  // Return the result
  setOutTensor(GroupNormOp::getYOutIndex(), y);
  setOutTensor(GroupNormOp::getMeanOutIndex(), mean);
  setOutTensor(GroupNormOp::getInvStdDevOutIndex(), invStdDev);
}

GroupNormGradOpx::GroupNormGradOpx(Op *op, Devicex *devicex)
    : NormOpx(op, devicex) {
  verifyOp<GroupNormGradOp>(op, Onnx::GradOperators::GroupNormalizationGrad);
}

void GroupNormGradOpx::grow(poplar::program::Sequence &prog) const {

  auto op = getOp<GroupNormGradOp>();

  auto x         = getInTensor(GroupNormGradOp::getXInIndex());
  auto yGrad     = getInTensor(GroupNormGradOp::getYGradInIndex());
  auto scale     = getInTensor(GroupNormGradOp::getScaleInIndex());
  auto mean      = getInTensor(GroupNormGradOp::getMeanInIndex());
  auto invStdDev = getInTensor(GroupNormGradOp::getInvStdDevInIndex());

  // Convert input shape to poplar rules
  poplar::Tensor xP, yGradP;
  poplar::Shape nonBroadcastDims;
  std::tie(xP, nonBroadcastDims)     = convertOnnxInputToPoplarInput(x);
  std::tie(yGradP, nonBroadcastDims) = convertOnnxInputToPoplarInput(yGrad);

  poplar::Tensor xWhitened = popnn::gn::groupNormWhiten(
      graph(), xP, mean, invStdDev, prog, idStr() + "/whitenedActs");

  // Compute the delta for the operand
  poplar::Tensor xGrad =
      popnn::gn::groupNormGradients(graph(),
                                    xWhitened,
                                    yGrad,
                                    invStdDev,
                                    scale,
                                    prog,
                                    poplar::FLOAT,
                                    idStr() + "/operandGrad");

  // Compute the deltas for scaled and offset
  poplar::Tensor scaleGrad;
  poplar::Tensor bGrad;
  std::tie(scaleGrad, bGrad) =
      popnn::gn::groupNormParamGradients(graph(),
                                         xWhitened,
                                         yGrad,
                                         prog,
                                         poplar::FLOAT,
                                         idStr() + "/scaleOffsetGrads");

  // Convert the output back into the input format
  xGrad = convertPoplarOutputToOnnxOutput(xGrad, nonBroadcastDims);

  // Return the result
  setOutTensor(GroupNormGradOp::getXGradOutIndex(), xGrad);
  setOutTensor(GroupNormGradOp::getScaleOutIndex(), scaleGrad);
  setOutTensor(GroupNormGradOp::getBOutIndex(), bGrad);
}

namespace {
OpxCreator<GroupNormOpx>
    groupNormOpxCreator({Onnx::CustomOperators::GroupNormalization_1});
OpxCreator<GroupNormGradOpx>
    groupNormGradOpxCreator(Onnx::GradOperators::GroupNormalizationGrad);
} // namespace

} // namespace popx
} // namespace poponnx
