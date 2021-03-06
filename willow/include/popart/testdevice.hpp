// Copyright (c) 2020 Graphcore Ltd. All rights reserved.
#ifndef GUARD_TEST_DEVICE_HPP
#define GUARD_TEST_DEVICE_HPP

#include <boost/variant.hpp>
#include <poplar/DeviceManager.hpp>
#include <poplar/Engine.hpp>
#include <poplar/IPUModel.hpp>
#include <poplar/OptionFlags.hpp>
#include <poplar/Target.hpp>
#include <poplar/TargetType.hpp>
#include <popart/ir.hpp>
#include <popart/names.hpp>

#include <chrono>
#include <iostream>
#include <thread>

namespace popart {

// In CMakeLists.txt there is a regex on "Hw*" so be
// careful when adding new enums that begin with Hw:
enum class TestDeviceType { Cpu, Sim, Sim2, Hw, IpuModel };

// Slight hack to stop IDEs complaining the TEST_TARGET is not defined.
#ifndef TEST_TARGET
static TestDeviceType TEST_TARGET;
#endif

constexpr bool isSimulator(TestDeviceType d) {
  return d == TestDeviceType::Sim;
}
constexpr bool isIpuModel(TestDeviceType d) {
  return d == TestDeviceType::IpuModel;
}
constexpr bool isHw(TestDeviceType d) { return d == TestDeviceType::Hw; }

std::shared_ptr<popart::DeviceInfo>
createTestDevice(const TestDeviceType testDeviceType,
                 const unsigned numIPUs          = 1,
                 const unsigned tilesPerIPU      = 0,
                 const SyncPattern pattern       = SyncPattern::Full,
                 const poplar::OptionFlags &opts = {}) {

  unsigned ipuModelTilesPerIPU = tilesPerIPU;

  if (ipuModelTilesPerIPU == 0 &&
      ((testDeviceType == TestDeviceType::Sim) ||
       (testDeviceType == TestDeviceType::IpuModel))) {
    // We need a number of tiles for these device types.
    ipuModelTilesPerIPU = 1216;
  }

  std::map<std::string, std::string> deviceOpts{
      {"numIPUs", std::to_string(numIPUs)},
      {"tilesPerIPU", std::to_string(ipuModelTilesPerIPU)}};

  switch (testDeviceType) {
  case TestDeviceType::Cpu:
    return DeviceManager::createDeviceManager().createCpuDevice();
  case TestDeviceType::Sim:
    return DeviceManager::createDeviceManager().createSimDevice(deviceOpts);
  case TestDeviceType::Hw:
    return DeviceManager::createDeviceManager().acquireAvailableDevice(
        numIPUs, tilesPerIPU, pattern);
  case TestDeviceType::IpuModel:
    return DeviceManager::createDeviceManager().createIpuModelDevice(
        deviceOpts);
  default:
    throw error("Unrecognized device {}", testDeviceType);
  }
}

inline const char *asString(const TestDeviceType &TestDeviceType) {
  switch (TestDeviceType) {
  case TestDeviceType::Cpu:
    return "Cpu";
  case TestDeviceType::IpuModel:
    return "IpuModel";
  case TestDeviceType::Sim:
    return "Sim";
  case TestDeviceType::Hw:
    return "Hw";
  default:
    break;
  }
  throw error("Invalid device type");
}

inline std::istream &operator>>(std::istream &is, TestDeviceType &type) {
  std::string token;
  is >> token;
  if (token == "Cpu")
    type = TestDeviceType::Cpu;
  else if (token == "IpuModel")
    type = TestDeviceType::IpuModel;
  else if (token == "Sim")
    type = TestDeviceType::Sim;
  else if (token == "Hw")
    type = TestDeviceType::Hw;
  else
    throw error("Unsupported device type <{}>; must be one of ('Cpu', "
                "'IpuModel', 'Sim' or 'Hw')XX",
                token);
  return is;
}

inline std::ostream &operator<<(std::ostream &os, const TestDeviceType &type) {
  os << asString(type);
  return os;
}
} // namespace popart

#endif // GUARD_TEST_DEVICE_HPP
