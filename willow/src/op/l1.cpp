#include <sstream>
#include <poponnx/error.hpp>
#include <poponnx/makeunique.hpp>
#include <poponnx/op/l1.hpp>
#include <poponnx/opmanager.hpp>
#include <poponnx/tensor.hpp>

namespace poponnx {

std::unique_ptr<Op> L1Op::clone() const { return make_unique<L1Op>(*this); }

std::vector<std::unique_ptr<Op>> L1Op::getGradOps() {
  std::vector<std::unique_ptr<Op>> upops;
  upops.emplace_back(make_unique<L1GradOp>(this));
  return upops;
}

std::unique_ptr<Op> L1Loss::getOp(Ir *gp) const {
  return std::unique_ptr<Op>(new L1Op(op_type(), gp, this));
}

const OperatorIdentifier &L1Loss::op_type() const {
  return Onnx::CustomOperators::L1;
}

std::vector<TensorId> L1Loss::getStreamTensorNames() const { return {}; }

L1Loss::L1Loss(TensorId in_, TensorId out_, float lmb)
    : Loss({in_}, out_), lambda(lmb) {}

TensorId L1Loss::getInputId() const { return input(0); }

float L1Loss::getLambda() const { return lambda; }

const L1Loss *L1Op::l1l() const { return l1loss_; }
const L1Loss *L1GradOp::l1l() const { return l1loss_; }

L1Op::L1Op(const OperatorIdentifier &_opid, Ir *_ir, const L1Loss *n)
    : LossOp(_opid, _ir), l1loss_(n) {}

void L1GradOp::setup() {
  // gradient of input has same shape as input to L1
  outInfo(getOutIndex()) = inInfo(getInIndex());
}

void L1Op::setup() {
  // output is a vector of length=batchsize, of the same type as input
  TensorInfo info0 = inInfo(getInIndex());
  if (info0.rank() == 0) {
    throw error("L1Op not valid for rank-0 tensor (scalar)");
  }
  int64_t batchsize = info0.dim(0);
  outInfo(getOutIndex()).set(info0.dataType(), {batchsize});
}

L1GradOp::L1GradOp(L1Op *op_)
    : Op(Onnx::CustomGradOperators::L1Grad, op_->pir), l1loss_(op_->l1l()) {}

const std::vector<GradInOutMapper> &L1GradOp::gradInputInfo() const {
  // input at index 0 of this grad op is the input at index 0 of the L1
  // non-grad op.
  static const std::vector<GradInOutMapper> inInfo = {
      {getInIndex(), L1Op::getInIndex(), GradOpInType::IN}};
  return inInfo;
}

const std::map<int, int> &L1GradOp::gradOutToNonGradIn() const {
  // grad-op's (only) output corresponds to op's (only) input.
  static const std::map<int, int> outInfo = {
      {getOutIndex(), L1Op::getInIndex()}};
  return outInfo;
}

namespace {
static LossOpCreator<L1Op> l1OpCreator(Onnx::CustomOperators::L1);
static GradOpCreator<L1GradOp>
    l1GradOpCreator(Onnx::CustomGradOperators::L1Grad);
} // namespace

} // namespace poponnx
