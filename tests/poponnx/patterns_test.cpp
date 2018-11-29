#define BOOST_TEST_MODULE PatternsTest

#include <boost/test/unit_test.hpp>
#include <vector>
#include <poponnx/builder.hpp>
#include <poponnx/dataflow.hpp>
#include <poponnx/earlyinfo.hpp>
#include <poponnx/filereader.hpp>
#include <poponnx/op/l1.hpp>
#include <poponnx/op/nll.hpp>
#include <poponnx/optimizer.hpp>
#include <poponnx/tensor.hpp>
#include <poponnx/tensorinfo.hpp>

using namespace poponnx;

BOOST_AUTO_TEST_CASE(PostNRepl_IdentityOp) {
  // clang-format off
  //
  // (*) -> [Identity] -> () -> [Identity] -> (*) 
  //     -> [Identity] -> () -> [Identity] ->  () 
  //     -> [Identity] -> () -> [Identity] ->  () -> [Identity] -> (*)
  //
  // where (*) are Anchors should become
  //
  // (*) -> [Identity] -> (*) -> [Identity] -> (*)
  //
  // clang-format on

  // Build an onnx model
  auto builder = Builder::create();

  TensorInfo shape{"FLOAT", std::vector<int64_t>{2}};

  auto i1 = builder->addInputTensor(shape);
  std::vector<TensorId> tensorIds{i1};
  // Create a chain of identity ops
  for (int i = 0; i < 6; i++) {
    auto x = builder->identity({tensorIds[tensorIds.size() - 1]});
    tensorIds.push_back(x);
  }
  builder->addOutputTensor(tensorIds.back());

  auto proto      = builder->getModelProto();
  auto modelProto = io::getModelFromString(proto);

  // Create the IR
  auto earlyInfo = EarlyInfo();
  earlyInfo.add(i1, shape);
  // Add the last tensor, and the 3rd tensor as anchors
  auto dataFlow =
      DataFlow(1, 1, {tensorIds.back(), tensorIds[2]}, AnchorReturnType::ALL);
  auto optimizer = SGD(0.01);
  std::vector<Loss *> losses{new L1Loss(tensorIds.back(), "l1LossVal", 0.1)};

  Ir ir;
  ir.prepare({modelProto,
              earlyInfo,
              dataFlow,
              losses,
              &optimizer,
              {},
              ".",
              {},
              {"PostNRepl"}});

  // Check the ir
  // All but one of the identityOps should have been removed from the ir
  BOOST_CHECK(ir.opsOfType(OpType::IDENTITY).size() == 2);

  // All but the 1st, 3rd and last tensors should have been removed
  for (int i = 0; i < tensorIds.size(); i++) {
    bool tensorExists = ir.getTensors().contains(tensorIds[i]);
    bool shouldExist  = i == 0 | i == 2 | i == 6;
    BOOST_CHECK(tensorExists == shouldExist);
  }
}

BOOST_AUTO_TEST_CASE(PreUniRepl) {
  // {(i1), (i2)} -> [Add] -> () -> [Pad] -> () -> [Identity] -> (identOut)
  //
  // should become
  //
  // {(i1), (i2)} -> [Add] -> () -> [Identity] -> (identOut)

  // Build an onnx model
  auto builder = Builder::create();

  TensorInfo shape{"FLOAT", std::vector<int64_t>{2}};

  auto input1 = builder->addInputTensor(shape);
  auto input2 = builder->addInputTensor(shape);

  auto padIn    = builder->add({input1, input2});
  auto padOut   = builder->pad({padIn}, "constant", {0, 0}, 0.0);
  auto identOut = builder->identity({padOut});

  builder->addOutputTensor(identOut);

  auto proto      = builder->getModelProto();
  auto modelProto = io::getModelFromString(proto);

  // Create the IR
  auto earlyInfo = EarlyInfo();
  earlyInfo.add(input1, shape);
  earlyInfo.add(input2, shape);
  // Add the last tensor, and the 3rd tensor as anchors
  auto dataFlow  = DataFlow(1, 1, {identOut}, AnchorReturnType::ALL);
  auto optimizer = SGD(0.01);
  std::vector<Loss *> losses{new L1Loss(identOut, "l1LossVal", 0.1)};

  Ir ir;
  ir.prepare({modelProto,
              earlyInfo,
              dataFlow,
              losses,
              &optimizer,
              {},
              ".",
              {},
              {"PreUniRepl"}});

  // Check the ir
  // the PadOp should have been removed
  BOOST_CHECK(ir.opsOfType(OpType::PAD).size() == 0);
  // padIn should have been removed
  BOOST_CHECK(ir.getTensors().contains(padIn) == false);
}

BOOST_AUTO_TEST_CASE(OpToIdentity) {
  // {(i1), (i2)} -> [Add] -> () -> [Pad] -> () -> [Identity] -> (identOut)
  //
  // should become
  //
  // {(i1), (i2)} -> [Add] -> () -> [Identity] () -> [Identity] -> (identOut)

  // Build an onnx model
  auto builder = Builder::create();

  TensorInfo shape{"FLOAT", std::vector<int64_t>{2}};

  auto input1 = builder->addInputTensor(shape);
  auto input2 = builder->addInputTensor(shape);

  auto padIn    = builder->add({input1, input2});
  auto padOut   = builder->pad({padIn}, "constant", {0, 0}, 0.0);
  auto identOut = builder->identity({padOut});

  builder->addOutputTensor(identOut);

  auto proto      = builder->getModelProto();
  auto modelProto = io::getModelFromString(proto);

  // Create the IR
  auto earlyInfo = EarlyInfo();
  earlyInfo.add(input1, shape);
  earlyInfo.add(input2, shape);
  // Add the last tensor, and the 3rd tensor as anchors
  auto dataFlow  = DataFlow(1, 1, {identOut}, AnchorReturnType::ALL);
  auto optimizer = SGD(0.01);
  std::vector<Loss *> losses{new L1Loss(identOut, "l1LossVal", 0.1)};

  Ir ir;
  ir.prepare({modelProto,
              earlyInfo,
              dataFlow,
              losses,
              &optimizer,
              {},
              ".",
              {},
              {"OpToIdentity"}});

  // Check the ir
  // the PadOp should have been replaced with an IdentityOp
  BOOST_CHECK(ir.opsOfType(OpType::PAD).size() == 0);
  BOOST_CHECK(ir.opsOfType(OpType::IDENTITY).size() == 2);
}

BOOST_AUTO_TEST_CASE(SplitConvBias) {
  // {(i1), (i2), (i3)} -> [Conv] -> () -> [Identity] -> (identOut)
  //
  // should become
  //
  // {(i1), (i2)} -> [Conv] -> (convOut)
  // {(i3), (convOut)} -> [AddBias] () -> [Identity] -> (identOut)

  // Build an onnx model
  auto builder = Builder::create();

  TensorInfo shape{"FLOAT", std::vector<int64_t>{1, 2, 2}};

  auto input1 = builder->addInputTensor(shape);
  auto input2 = builder->addInputTensor(shape);
  auto input3 = builder->addInputTensor(shape);

  auto convOut = builder->convolution(
      {input1, input2, input3}, {1, 1}, {0, 0, 0, 0}, {1, 1}, 1, false);
  auto identOut = builder->identity({convOut});

  builder->addOutputTensor(identOut);

  auto proto      = builder->getModelProto();
  auto modelProto = io::getModelFromString(proto);

  // Create the IR
  auto earlyInfo = EarlyInfo();
  earlyInfo.add(input1, shape);
  earlyInfo.add(input2, shape);
  earlyInfo.add(input3, shape);
  // Add the last tensor, and the 3rd tensor as anchors
  auto dataFlow  = DataFlow(1, 1, {identOut}, AnchorReturnType::ALL);
  auto optimizer = SGD(0.01);
  std::vector<Loss *> losses{new L1Loss(identOut, "l1LossVal", 0.1)};

  Ir ir;
  ir.prepare({modelProto,
              earlyInfo,
              dataFlow,
              losses,
              &optimizer,
              {},
              ".",
              {},
              {"SplitConvBias"}});

  // Check the ir
  // Input 1 should connect to ConvOp
  // ConvOp should only have 2 inputs
  auto input1Tensor = ir.getTensors().get(input1);
  auto convOp       = input1Tensor->consumers.getOps()[0];
  BOOST_CHECK(convOp->input.n() == 2);

  auto bias = convOp->output.tensor(0)->consumers.getOps()[0];
  BOOST_CHECK(bias->op_type() == "AddBias");

  // Input3 should be consumed only by the AddBiasOp
  auto input3Tensor = ir.getTensors().get(input3);
  BOOST_CHECK(input3Tensor->consumers.getTotal() == 1);
  BOOST_CHECK(bias == input3Tensor->consumers.getOps()[0]);
}

BOOST_AUTO_TEST_CASE(SubtractArg1GradOp) {
  // () -> [SubtractGradArg1Op] -> ()
  //
  // should become
  //
  // () -> [Negate] -> () -> [ReduceSum] -> ()
  // Build an onnx model
  auto builder = Builder::create();

  TensorInfo shape{"FLOAT", std::vector<int64_t>{2}};

  auto input1 = builder->addInputTensor(shape);
  auto input2 = builder->addInputTensor(shape);

  auto subtractOut = builder->sub({input1, input2});
  auto identOut    = builder->identity({subtractOut});

  builder->addOutputTensor(identOut);

  auto proto      = builder->getModelProto();
  auto modelProto = io::getModelFromString(proto);

  // Create the IR
  auto earlyInfo = EarlyInfo();
  earlyInfo.add(input1, shape);
  earlyInfo.add(input2, shape);
  // Add the last tensor, and the 3rd tensor as anchors
  auto dataFlow  = DataFlow(1,
                           1,
                           {identOut,
                            reservedGradientPrefix() + input1,
                            reservedGradientPrefix() + input2},
                           AnchorReturnType::ALL);
  auto optimizer = SGD(0.01);
  std::vector<Loss *> losses{new L1Loss(identOut, "l1LossVal", 0.1)};

  Ir ir;
  ir.prepare({modelProto,
              earlyInfo,
              dataFlow,
              losses,
              &optimizer,
              {},
              ".",
              {},
              {"SubtractArg1GradOp"}});

  // Check the ir
  // SubtractArg1Grad should have been replaced with Negate and ReduceSum
  BOOST_CHECK(ir.opsOfType(OpType::SUBTRACTARG1GRAD).size() == 0);
  BOOST_CHECK(ir.opsOfType(OpType::NEGATE).size() == 1);
  BOOST_CHECK(ir.opsOfType(OpType::REDUCESUM).size() == 1);
}

BOOST_AUTO_TEST_CASE(SoftmaxGradDirect) {
  // (label), (probs) -> [NLLGrad]
  // [NllGrad] -> (d_probs)
  // (d_probs), (probs) -> [SoftmaxGrad] -> (d_acts)
  //
  // should become
  //
  // (label), (probs) -> [SoftmaxGradDirect] -> (d_acts)

  // Build an onnx model
  auto builder = Builder::create();

  TensorInfo shape{"FLOAT", std::vector<int64_t>{2}};

  auto input1 = builder->addInputTensor(shape);
  auto input2 = builder->addInputTensor(shape);

  auto identOut   = builder->identity({input1});
  auto softmaxOut = builder->softmax({identOut});

  builder->addOutputTensor(softmaxOut);

  auto proto      = builder->getModelProto();
  auto modelProto = io::getModelFromString(proto);

  // Create the IR
  auto earlyInfo = EarlyInfo();
  earlyInfo.add(input1, shape);
  earlyInfo.add(input2, shape);
  // Add the last tensor, and the 3rd tensor as anchors
  auto dataFlow =
      DataFlow(1,
               1,
               {softmaxOut, reservedGradientPrefix() + input1, "nllLossVal"},
               AnchorReturnType::ALL);
  auto optimizer = SGD(0.01);
  std::vector<Loss *> losses{new NllLoss(softmaxOut, input2, "nllLossVal")};

  auto opts      = SessionOptions();
  opts.exportDot = true;

  Ir ir;
  ir.prepare({modelProto,
              earlyInfo,
              dataFlow,
              losses,
              &optimizer,
              {},
              ".",
              opts,
              {"PreUniRepl", "SoftmaxGradDirect"}});

  // Check the ir
  // NllGradOp and SoftmaxGradOp should have been replaced with
  // SoftmaxGradDirectOp
  BOOST_CHECK(ir.opsOfType(OpType::NLLGRAD).size() == 0);
  BOOST_CHECK(ir.opsOfType(OpType::SOFTMAXGRAD).size() == 0);
  BOOST_CHECK(ir.opsOfType(OpType::SOFTMAXGRADDIRECT).size() == 1);
}

// where we test that a series of Relus is converted
// into InplaceRelus by the Inplace0 pattern.
BOOST_AUTO_TEST_CASE(Inplace0_series) {

  // Consider the SERIES of Relu Ops:
  //
  // (in0) -> [Relu] -> (h0)
  //       -> [Relu] -> (h1)
  //       -> [Relu] -> (preId)
  //       -> [Identity] -> (out),
  //
  // with (out) as an anchor tensor. This should become,
  //
  // (in0) -> {[ReluInplace], [ReluInplace], [ReluInplace]}
  // (in0) -> [Identity] -> (out).

  // Build an onnx model
  auto builder = Builder::create();

  TensorInfo shape{"FLOAT", std::vector<int64_t>{1}};
  auto in0   = builder->addInputTensor(shape);
  auto h0    = builder->relu({in0});
  auto h1    = builder->relu({h0});
  auto preId = builder->relu({h1});
  auto out   = builder->identity({preId});
  builder->addOutputTensor(out);

  auto proto      = builder->getModelProto();
  auto modelProto = io::getModelFromString(proto);

  // Create the IR
  auto earlyInfo = EarlyInfo();
  earlyInfo.add(in0, shape);
  std::vector<TensorId> anchors{out};
  auto dataFlow  = DataFlow(1, 1, anchors, AnchorReturnType::ALL);
  auto optimizer = SGD(0.01);
  std::vector<Loss *> losses{new L1Loss(out, "l1LossVal", 0.1)};

  Ir ir;
  ir.prepare({modelProto,
              earlyInfo,
              dataFlow,
              losses,
              &optimizer,
              {},
              ".",
              {},
              {"Inplace0"}});

  // Check the ir
  // All the Relus have been optimised out,
  BOOST_CHECK(ir.opsOfType(OpType::RELU).size() == 0);
  // and have been replaced with ReluInplace.
  BOOST_CHECK(ir.opsOfType(OpType::RELUINPLACE).size() == 3);
}

// where we test that with Relus is parallel, exactly 1 of
// them is converted into an InplaceRelu by the Inplace0 pattern.
BOOST_AUTO_TEST_CASE(Inplace0_parallel) {

  // Consider the Relu Ops in PARALLEL
  //
  //           | -- [Relu] -- (h0) -- |
  //           |                      | --- [Add] -- (h3) -|
  // (in0) >---| -- [Relu] -- (h1) -- |                    |
  //           |                                           | -> [Add] -- (out)
  //           | -- [Relu] -- (h2) ----------------------- |
  //
  // We can make the first relu in-place, but then stall as
  // an in-place op must run after all other consumers (and
  // therefore there can only be one in-place consumer here). So, we expect:
  //
  //           | -------------------- |
  //           |                      |
  //           | -- [ReluInplace]     |
  //           |                      | --- [Add] -- (h3) -|
  // (in0) >---| -- [Relu] -- (h1) -- |                    |
  //           |                                           | -> [Add] -- (out)
  //           | -- [Relu] -- (h2) ----------------------- |
  //
  //

  // Build an onnx model
  auto builder = Builder::create();

  TensorInfo shape{"FLOAT", std::vector<int64_t>{1}};
  auto in0 = builder->addInputTensor(shape);
  auto h0  = builder->relu({in0});
  auto h1  = builder->relu({in0});
  auto h2  = builder->relu({in0});
  auto h3  = builder->add({h0, h1});
  auto out = builder->add({h2, h3});
  builder->addOutputTensor(out);

  auto proto      = builder->getModelProto();
  auto modelProto = io::getModelFromString(proto);

  // Create the IR
  auto earlyInfo = EarlyInfo();
  earlyInfo.add(in0, shape);
  std::vector<TensorId> anchors{out};
  auto dataFlow  = DataFlow(1, 1, anchors, AnchorReturnType::ALL);
  auto optimizer = SGD(0.01);
  std::vector<Loss *> losses{new L1Loss(out, "l1LossVal", 0.1)};

  Ir ir;
  ir.prepare({modelProto,
              earlyInfo,
              dataFlow,
              losses,
              &optimizer,
              {},
              ".",
              {},
              {"Inplace0"}});

  // Check the ir
  // All the Relus have been optimised out,
  BOOST_CHECK(ir.opsOfType(OpType::RELU).size() == 3 - 1);
  // and have been replaced with ReluInplace.
  BOOST_CHECK(ir.opsOfType(OpType::RELUINPLACE).size() == 1);
}
