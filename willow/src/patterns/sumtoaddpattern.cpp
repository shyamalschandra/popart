#include <poponnx/ir.hpp>
#include <poponnx/makeunique.hpp>
#include <poponnx/op/add.hpp>
#include <poponnx/op/sum.hpp>
#include <poponnx/patterns/sumtoaddpattern.hpp>
#include <poponnx/tensor.hpp>
#include <poponnx/tensorinfo.hpp>

namespace poponnx {

bool SumToAddPattern::matches(Op *op) const {
  return op->isConvertibleTo<SumOp>() && op->input->n() == 2;
}

std::vector<const Tensor *> SumToAddPattern::touches(Op *) const { return {}; }

// grad_out = grad_in / fwd_in1
bool SumToAddPattern::apply(Op *op) const {
  // have already verified that op has 2 inputs in ::matches
  auto inputs = op->input->tensors();
  auto out    = op->outTensor(SumOp::getOutIndex());

  // Remove SumOp
  op->disconnectAllInputs();
  op->disconnectAllOutputs();
  op->getIr().eraseOp(op->id);

  // create the new op
  auto add_op = makeReplacementOpInIr(Onnx::AiOnnx::OpSet9::Add, op);

  // connect the new op
  add_op->connectInTensor(AddOp::getArg0InIndex(), inputs[0]->id);
  add_op->connectInTensor(AddOp::getArg1InIndex(), inputs[1]->id);
  add_op->connectOutTensor(AddOp::getOutIndex(), out->id);

  return true;
}

namespace {
static PatternCreator<SumToAddPattern>
    SumToAddPattern(PreAliasPatternType::SUMTOADD, "SumToAdd");
}

} // namespace poponnx