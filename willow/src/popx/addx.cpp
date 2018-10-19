#include <willow/add.hpp>
#include <willow/error.hpp>
#include <willow/popx/addx.hpp>

namespace willow {
namespace popx {

AddOpx::AddOpx(Op *op) : Opx(op) {
  if (op->opType != OpType::ADD) {
    throw error("cannot create AddOpx from " + op->op_type());
  }
}

AddOp *AddOpx::getAddOp() const { return dynamic_cast<AddOp *>(getOp()); }

} // namespace popx
} // namespace willow