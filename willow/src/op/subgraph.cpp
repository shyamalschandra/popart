
#include <poponnx/ir.hpp>
#include <poponnx/op/subgraph.hpp>
#include <poponnx/opserialiser.hpp>

namespace poponnx {

SubgraphOp::SubgraphOp(Graph &graph_, int64_t cacheId_)
    : Op(Onnx::CustomOperators::Subgraph, {graph_, ""}), cacheId(cacheId_) {}

std::vector<Op *> SubgraphOp::getOps() {
  std::vector<Op *> o;
  for (auto &_o : getChildOpsInfo()) {
    o.push_back(_o.op);
  }
  return o;
}

void SubgraphOp::appendAttributes(OpSerialiserBase &os) const {
  Op::appendAttributes(os);
  os.appendAttribute("cacheId", cacheId);
}

std::pair<SubgraphOp::OpInfo const &, InIndex>
SubgraphOp::getOpInfo(InIndex inIndex) const {

  InIndex counter = 0;
  InIndex delta   = 0;
  for (auto &child : childOpsInfo) {

    delta = counter;

    for (auto input : child.inputs) {

      if (counter++ == inIndex) {
        return std::make_pair(std::ref(child), inIndex - delta);
      }
    }
  }

  throw error("Could not find input {} in subgraph", inIndex);
}

view::Region SubgraphOp::modifies(InIndex inIndex) const {
  auto op = getOpInfo(inIndex);
  return op.first.op->modifies(op.second);
}
view::Region SubgraphOp::uses(InIndex inIndex) const {
  auto op = getOpInfo(inIndex);
  return op.first.op->uses(op.second);
}
view::Region SubgraphOp::aliases(InIndex inIndex) const {
  auto op = getOpInfo(inIndex);
  return op.first.op->aliases(op.second);
}

} // namespace poponnx
