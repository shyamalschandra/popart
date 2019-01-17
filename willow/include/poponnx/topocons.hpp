#ifndef GUARD_NEURALNET_TOPOCONS_HPP
#define GUARD_NEURALNET_TOPOCONS_HPP

#include <poponnx/names.hpp>
namespace poponnx {

// Topological constraints
//
// A topological constraint is a single edge between
// 2 Ops, stating the relative order they must appear
// in when scheduled (topologically sorted).
// we use a -> b to denote tnat a must be scheduled before b.
// These constraints are needed to support ops
// where input and output tensors share memory, such as
// in-place ops, view-changing ops, and weight-update ops.

class TopoCons {
public:
  // insert topological constraints such that "last"
  // is guaranteed to run after all other consumers
  // of Tensor "consumed"
  void setFinalConsumer(const Tensor *consumed, Op *last);

  // remove all topological constraints with op in it
  void remove(Op *op);

  // insert the constraint "before -> after"
  // if already present, do nothing
  void insert(Op *before, Op *after);

  // replace all topological constraints involving "beforeTransfer"
  // with "afterTransfer", on both ends of topological constraints
  void transfer(Op *beforeTransfer, Op *afterTransfer);

  bool contains(Op *before, Op *after) const;
  std::vector<Op *> getAfters(Op *before) const;
  std::vector<Op *> getBefores(Op *after) const;

private:
  // for all val : set, "key -> val"
  std::map<Op *, std::set<Op *>> valsAfter;

  // the mirror of valsAfterKey, so for all val : set, "val -> key"
  std::map<Op *, std::set<Op *>> valsBefore;
};
} // namespace poponnx

#endif
