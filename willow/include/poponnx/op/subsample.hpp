#ifndef GUARD_NEURALNET_SUBSAMPLE_HPP
#define GUARD_NEURALNET_SUBSAMPLE_HPP

#include <poponnx/op.hpp>

namespace poponnx {

class SubsampleOp : public Op {
public:
  SubsampleOp(const OperatorIdentifier &_opid,
              const std::vector<int64_t> &strides_,
              const Op::Settings &settings_);
  std::unique_ptr<Op> clone() const override;
  std::vector<std::unique_ptr<Op>> getGradOps() final;
  void setup() override;

  static InIndex getInIndex() { return 0; }
  static OutIndex getOutIndex() { return 0; }

  // The stride is a vector whose length is the rank of the input tensor
  // If strides is defined as {1,..,1} the the input tensor will not be changed
  std::vector<uint32_t> strides_u32() const;

  // Returns true if all the strides at 1
  bool strideSizeOne() const;

  void appendAttributes(std::stringstream &ss,
                        const std::string &tab) const override;

public:
  std::vector<int64_t> strides;
};

class SubsampleGradOp : public Op {
public:
  SubsampleGradOp(const SubsampleOp &fwdOp);
  std::unique_ptr<Op> clone() const final;
  void setup() override;

  const std::vector<GradInOutMapper> &gradInputInfo() const final;
  const std::map<int, int> &gradOutToNonGradIn() const final;

  static InIndex getInIndex() { return 0; }
  static OutIndex getOutIndex() { return 0; }

  const SubsampleOp &getFwdOp() const { return fwdOp; }

private:
  const SubsampleOp &fwdOp;
  TensorInfo fwdOpInfo;
};

} // namespace poponnx

#endif
