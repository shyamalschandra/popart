#include <numeric>
#include <onnx/onnx_pb.h>
#include <poponnx/error.hpp>
#include <poponnx/graph.hpp>
#include <poponnx/makeunique.hpp>
#include <poponnx/op/tile.hpp>
#include <poponnx/opmanager.hpp>
#include <poponnx/tensor.hpp>
#include <poponnx/tensors.hpp>

namespace poponnx {

// This will be used by TileGradOp
TileOp::TileOp(const OperatorIdentifier &_opid,
               const Shape &ots,
               const std::vector<int64_t> &rps,
               const Op::Settings &settings_)
    : Op(_opid, settings_), outShape(ots), repeats(rps) {}

TileOp::TileOp(const OperatorIdentifier &_opid, const Op::Settings &settings_)
    : Op(_opid, settings_) {}

const Shape &TileOp::getOutShape() { return outShape; }

const std::vector<int64_t> &TileOp::getRepeats() const { return repeats; }

std::vector<std::unique_ptr<Op>> TileOp::getGradOps() {
  std::vector<std::unique_ptr<Op>> upops;
  upops.emplace_back(make_unique<TileGradOp>(*this));
  return upops;
}

std::unique_ptr<Op> TileOp::clone() const { return make_unique<TileOp>(*this); }

void TileOp::setup() {
  // output type  : same as input type;
  // output shape : outShape, determined in the constructor
  outInfo(getOutIndex()) = {inInfo(getInIndex()).dataType(), outShape};
}

void TileOp::connectInTensor(InIndex inIndex, TensorId tenId) {
  // index 0 is the data tensor to be tiled. We connect
  // the data tensor to this Op as an input, the default
  // connection of an input tensor to its Op
  if (inIndex == 0) {
    defaultConnectInTensor(inIndex, tenId);
  } else if (inIndex == 1) {
    // we attempt to set outputInfo

    TensorId repeatId = tenId;

    // check 2 : that there is already a tensor with the shape tensor's name
    if (!getGraph().getTensors().contains(repeatId)) {
      throw error("no Tensor named `" + repeatId + "' recorded in Ir. " +
                  " This is the second input in the TileOp constructor. ");
    }
    Tensor *repeatTensor = getGraph().getTensors().get(repeatId);

    // check 3 : that the tensor is of type constant
    if (repeatTensor->tensorType() != TensorType::Const) {
      throw error("The 'Repeats' Tensor `" + repeatId +
                  "' must be of type Constant");
    }

    // check 4 : that the tensor has data
    if (!repeatTensor->hasTensorData()) {
      throw error("The 'Repeats' Tensor `" + repeatId + "' does not have data");
    }
    TensorData *tensorData = repeatTensor->tensorData();

    // check 5 : that the data is int64 (as per the ONNX spec)
    if (repeatTensor->info.dataType() != DataType::INT64) {
      throw error("'Repeats' tensor `" + repeatId + "' is not INT64, it is " +
                  repeatTensor->info.data_type());
    }

    // check 6 : that it is rank 1
    if (repeatTensor->info.rank() != 1) {
      throw error("'Repeats' tensor `" + repeatId +
                  "' should be rank 1 in TileOp constructor");
    }

    // check 7 : that its length is same as rank of input data tensor
    if (repeatTensor->info.nelms() != inInfo(getInIndex()).rank()) {
      throw error(
          "'Repeats' tensor `" + repeatId +
          "' should have one element for each dimension of the data tensor");
    }

    // Finally, we can set the shape of the output tensor
    outShape        = {};
    repeats         = {};
    auto inputShape = inShape(getInIndex());
    int64_t *data   = static_cast<int64_t *>(tensorData->data());
    for (int i = 0; i < repeatTensor->info.dim(0); ++i) {
      if (data[i] < 1) {
        throw error("'Repeats' tensor `" + repeatId + "' has invalid value `" +
                    std::to_string(data[i]) + "' at index " +
                    std::to_string(i));
      }
      outShape.push_back(inputShape[i] * data[i]);
      repeats.push_back(data[i]);
    }
  } else {
    throw error("Unexpected index " + std::to_string(inIndex) +
                " in TileOp::connectInTensor");
  }
}

TileGradOp::TileGradOp(const TileOp &op_)
    : TileOp(Onnx::GradOperators::TileGrad,
             // the output shape of this bwd op is the input shape of the fwd op
             op_.inInfo(TileOp::getInIndex()).shape(),
             op_.getRepeats(),
             op_.getSettings()) {}

const std::vector<GradInOutMapper> &TileGradOp::gradInputInfo() const {
  // input at index 0 : gradient of output of tile
  static const std::vector<GradInOutMapper> inInfo = {
      {getInIndex(), TileOp::getOutIndex(), GradOpInType::GRADOUT}};
  return inInfo;
}

const std::map<int, int> &TileGradOp::gradOutToNonGradIn() const {
  // the grad-op's output at index 0 corresponds
  // to the non-grad-op's input at index 0
  static const std::map<int, int> outInfo = {
      {getOutIndex(), TileOp::getInIndex()}};
  return outInfo;
}

bool TileOp::canBeReplacedByIdentity() {
  return inShape(getInIndex()) == outShape;
}

namespace {
static OpCreator<TileOp> tileOpCreator({Onnx::Operators::Tile_1,
                                        Onnx::Operators::Tile_6});
} // namespace

} // namespace poponnx