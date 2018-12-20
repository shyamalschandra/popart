#include <popops/ElementWise.hpp>
#include <poponnx/error.hpp>
#include <poponnx/op/reciprocal.hpp>
#include <poponnx/popx/devicex.hpp>
#include <poponnx/popx/op/reciprocalx.hpp>
#include <poponnx/popx/opxmanager.hpp>
#include <poponnx/tensor.hpp>

namespace poponnx {
namespace popx {

ReciprocalOpx::ReciprocalOpx(Op *op, Devicex *devicex) : Opx(op, devicex) {
  verifyOp<ReciprocalOp>(op, Onnx::Operators::Reciprocal);
}

void ReciprocalOpx::grow(poplar::program::Sequence &prog) const {
  auto ones = dv_p->getConst(popType(op_p->inInfo(0)), {1}, 1.0);

  insert(outId(0),
         popops::map(graph(),
                     popops::expr::BinaryOpType::DIVIDE,
                     ones,
                     get(inId(0)),
                     prog,
                     idStr()));
}

namespace {
OpxCreator<ReciprocalOpx> reciprocalOpxCreator(Onnx::Operators::Reciprocal);
OpxCreator<Opx> reciprocalGradGradOpxCreator(
    Onnx::GradOperators::ReciprocalGrad,
    "ReciprocalGradOpx should be removed by pattern 'ReciprocalGradOpx'");
} // namespace

} // namespace popx
} // namespace poponnx
