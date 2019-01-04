import sys
import os
import numpy as np
import tempfile
import poponnx
from poponnx.torch import torchwriter
# we require torch in this file to create the torch Module
import torch
from torchvision import transforms, datasets
import re
from tempfile import TemporaryDirectory


def get_trainset():
    tmpdir = tempfile.gettempdir()

    c10datadir = os.path.abspath(os.path.join(tmpdir, 'cifar10data'))
    if (not os.path.exists(c10datadir)):
        print("Creating directory %s" % (c10datadir))
        os.mkdir(c10datadir)

    transform = transforms.Compose([
        transforms.ToTensor(),
        transforms.Normalize((0.5, 0.5, 0.5), (0.5, 0.5, 0.5))
    ])

    trainset = datasets.CIFAR10(
        root=c10datadir, train=True, download=True, transform=transform)

    return trainset


def get_session(fnModel, inputShapeInfo, dataFeed, torchWriter, passes, opts):
    # Reads ONNX model from file and creates backwards graph,
    # performs Ir optimisations
    session = poponnx.Session(
        fnModel=fnModel,
        inputShapeInfo=inputShapeInfo,
        dataFeed=dataFeed,
        losses=torchWriter.losses,
        optimizer=torchWriter.optimizer,
        passes=passes,
        userOptions=opts)

    print("Setting device to IPU, and preparing it")
    session.setDevice(poponnx.DeviceManager().createCpuDevice())
    session.prepareDevice()

    print("Writing weights to device")
    session.weightsFromHost()

    print("Writing Optimizer tensors to device, if there are any")
    session.optimizerFromHost()

    return session


def compare_models(model_A0, model_A1, model_B0, model_B1):
    report = poponnx.NumericsReport(model_A0, model_A1, model_B0, model_B1)
    report = report.fullReport()
    print(report)

    difference = 0.0

    for line in report.splitlines():
        match = re.search(' =\s+(\d+)', line)
        if match:
            difference += float(match.group(1))

    return difference


def run(torchWriter, passes, outputdir, cifarInIndices):
    dataFeed = torchWriter.dataFeed
    inputShapeInfo = torchWriter.inputShapeInfo

    fnModel0 = os.path.join(outputdir, "model0.onnx")

    # write ONNX Model to file
    torchWriter.saveModel(fnModel=fnModel0)

    trainset = get_trainset()

    stepLoader = torch.utils.data.DataLoader(
        trainset,
        # the amount of data loaded for each step.
        # note this is not the batch size, it's the "step" size
        # (samples per step)
        batch_size=torchWriter.samplesPerBatch * dataFeed.batchesPerStep(),
        shuffle=False,
        num_workers=3)

    opts = poponnx.SessionOptionsCore()
    opts.exportDot = True
    opts.logging = {"all": "TRACE", "session": "TRACE"}
    opts.logDir = outputdir

    session = get_session(fnModel0, inputShapeInfo, dataFeed, torchWriter,
                          passes, opts)

    def addStepDimension(data, batchesPerStep):
        if batchesPerStep == 1:
            return data
        else:
            dataShape = np.array(np.shape(data))
            dataShape[0] //= batchesPerStep
            dataShape = np.insert(dataShape, 0, batchesPerStep)
            return np.reshape(data, dataShape)

    stepi = 0
    for epoch in range(4):  # loop over the dataset multiple times
        for i, data in enumerate(stepLoader, 0):
            if i == 1:
                break

            inputs = {}
            for tenId in cifarInIndices.keys():
                inputs[tenId] = \
                    addStepDimension(data[cifarInIndices[tenId]].numpy(),
                                     session.dataFeed.batchesPerStep())
            stepi += 1

            torchWriter.train(inputs)

    def getFnModel(framework, stepi):
        return os.path.join(outputdir, "%sModel_%d.onnx" % (framework, stepi))

    def getFnPopOnnx(stepi):
        return getFnModel("PopOnnx", stepi)

    def getFnTorch(stepi):
        return getFnModel("Torch", stepi)

    # save the pytorch model
    fnTorchModel = getFnTorch(0)
    torchWriter.saveModel(fnTorchModel)

    # save the poponnx model
    fnPopOnnxModel = getFnPopOnnx(0)
    session.modelToHost(fnPopOnnxModel)

    # check that the models do not match
    diff = compare_models(fnModel0, fnTorchModel, fnModel0, fnPopOnnxModel)
    assert (diff > 0.0)

    print("Updating weights using model %s" % (fnTorchModel, ))
    # update the weights from model fnTorchModel
    session.resetHostWeights(fnTorchModel)
    session.weightsFromHost()

    # save the poponnx model
    fnPopOnnxModel = getFnPopOnnx(1)
    session.modelToHost(fnPopOnnxModel)

    # check that the models match
    diff = compare_models(fnModel0, fnTorchModel, fnModel0, fnPopOnnxModel)
    assert (diff == 0.0)


nChans = 3

# process samplesPerBatch = 2 samples at a time,
# so weights updated on average gradient of
# samplesPerBatch = 2 samples. samplesPerBatch
# is EXACTLY the batch size.
samplesPerBatch = 2

# Return requested tensors every batchesPerStep = 3 cycles.
# so only communicate back to host every 2*3 = 6 samples.
batchesPerStep = 3

# anchors and how to return them : in this example,
# return the l1 loss "l1LossVal",
# the tensor to which the loss is applied "out",
# and the input tensor "image0"
anchors = {
    "l1LossVal": poponnx.AnchorReturnType("FINAL"),
    "out": poponnx.AnchorReturnType("FINAL"),
    "image0": poponnx.AnchorReturnType("FINAL")
}

dataFeed = poponnx.DataFlow(batchesPerStep, anchors)

# willow is non-dynamic. All input Tensor shapes and
# types must be fed into the Session constructor.
# In this example there is 1 streamed input, image0.
inputShapeInfo = poponnx.InputShapeInfo()
inputShapeInfo.add(
    "image0", poponnx.TensorInfo("FLOAT", [samplesPerBatch, nChans, 32, 32]))

inNames = ["image0"]

# outNames: not the same as anchors,
# outNames: not the same as anchors,
# these are the Tensors which will be
# connected to the loss layers
outNames = ["out"]

# cifar training data loader : at index 0 : image, at index 1 : label.
cifarInIndices = {"image0": 0}

losses = [poponnx.L1Loss("out", "l1LossVal", 0.1)]

# The optimization passes to run in the Ir, see patterns.hpp
willowOptPasses = poponnx.Patterns()


class Module0(torch.nn.Module):
    def __init__(self):
        torch.nn.Module.__init__(self)
        self.conv1 = torch.nn.Conv2d(
            nChans,
            nChans,
            kernel_size=(3, 3),
            stride=1,
            padding=(1, 3),
            bias=False)
        self.relu = torch.nn.functional.relu

    def forward(self, inputs):
        """out = relu(conv(in))"""
        image0 = inputs[0]
        x = self.conv1(image0)
        x = self.relu(x)
        return x


# Set arbitrary seed so model weights are initialized to the
# same values each time the test is run
torch.manual_seed(1)

torchWriter = torchwriter.PytorchNetWriter(
    inNames=inNames,
    outNames=outNames,
    losses=losses,
    optimizer=poponnx.ConstSGD(0.001),
    inputShapeInfo=inputShapeInfo,
    dataFeed=dataFeed,
    # Torch specific:
    module=Module0(),
    samplesPerBatch=samplesPerBatch)

if len(sys.argv) == 2:
    outputdir = sys.argv[1]
    run(torchWriter, willowOptPasses, outputdir, cifarInIndices)
else:
    with TemporaryDirectory() as outputdir:
        run(torchWriter, willowOptPasses, outputdir, cifarInIndices)
