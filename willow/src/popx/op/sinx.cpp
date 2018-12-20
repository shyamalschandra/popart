#include <popops/ElementWise.hpp>
#include <poponnx/error.hpp>
#include <poponnx/op/sin.hpp>
#include <poponnx/popx/op/sinx.hpp>
#include <poponnx/popx/opxmanager.hpp>

namespace poponnx {
namespace popx {

SinOpx::SinOpx(Op *op, Devicex *devicex) : Opx(op, devicex) {
  verifyOp<SinOp>(op, Onnx::Operators::Sin);
}

void SinOpx::grow(poplar::program::Sequence &prog) const {
  insert(outId(SinOp::getOutIndex()),
         popops::map(graph(),
                     popops::expr::UnaryOpType::SIN,
                     get(inId(SinOp::getInIndex())),
                     prog,
                     idStr()));
}

namespace {
OpxCreator<SinOpx> sinOpxCreator(Onnx::Operators::Sin);
OpxCreator<Opx> sinGradOpxCreator(
    Onnx::GradOperators::SinGrad,
    "SinGradOp should be optimised out, \"SinGradOp\" pattern is required");
} // namespace

} // namespace popx
} // namespace poponnx
