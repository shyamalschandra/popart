#include <willow/error.hpp>
#include <willow/nll.hpp>
#include <willow/popx/nllx.hpp>

namespace willow {
namespace popx {

NllOpx::NllOpx(Op *op, Devicex *devicex) : Opx(op, devicex) {
  if (op->opType != OpType::NLL) {
    throw error("cannot create NllOpx from " + op->op_type());
  }
}

NllOp *NllOpx::getNllOp() const { return dynamic_cast<NllOp *>(op_p); }

NllGradOpx::NllGradOpx(Op *op, Devicex *devicex) : Opx(op, devicex) {
  if (op->opType != OpType::NLLGRAD) {
    throw error("cannot create NllGradOpx from " + op->op_type());
  }
}

NllGradOp *NllGradOpx::getNllGradOp() const {
  return dynamic_cast<NllGradOp *>(op_p);
}

} // namespace popx
} // namespace willow
