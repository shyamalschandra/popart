// Copyright (c) 2018 Graphcore Ltd. All rights reserved.

#include <popart/graph.hpp>
#include <popart/opmanager.hpp>

namespace popart {

std::ostream &operator<<(std::ostream &os, const OperatorIdentifier &opid) {
  os << opid.domain << "." << opid.type << ":" << opid.version;
  return os;
}

const std::vector<TensorId> &OpCreatorInfo::getInputIds() const {
  if (inputIds.empty()) {
    throw internal_error(
        "No inputs ids were passed to the call to OpManager::createOp, but the "
        "op factory function for op {} is attempting to get the input ids.\n"
        "Consider checking the call to createOp and adding the input ids.",
        opid);
  } else {
    return inputIds;
  }
}

Tensor *OpCreatorInfo::getInputTensor(int index) const {
  auto id = inputIds.at(index);
  return settings.graph.get().getTensors().get(id);
}

OpManager &OpManager::getInstance() {
  static OpManager instance;
  return instance;
}

void OpManager::registerOp(const OperatorIdentifier &opid,
                           const OpDefinition &details,
                           bool isPublic,
                           OpFactoryFunc func) {

  OpInfo info(opid);
  info.isPublic = isPublic;
  info.f1       = func;
  info.details  = details;

  auto it = getInstance().opMap.find(std::make_pair(opid.domain, opid.type));
  if (it != getInstance().opMap.end()) {
    // Add to list
    it->second.insert(std::make_pair(opid.version, info));
  } else {
    // Add new entry for domain/type
    std::map<int, OpInfo> map;
    map.insert(std::make_pair(opid.version, info));
    getInstance().opMap.insert(
        std::make_pair(std::make_pair(opid.domain, opid.type), map));
  }
}

const std::vector<OperatorIdentifier>
OpManager::getSupportedOperations(bool includePrivate) {
  std::vector<OperatorIdentifier> list;

  for (auto &op : OpManager::getInstance().opMap) {
    for (auto &opVersion : op.second) {
      if (opVersion.second.isPublic || includePrivate) {
        list.push_back(opVersion.second.id);
      }
    }
  }

  return list;
}

const OpDefinitions
OpManager::getSupportedOperationsDefinition(bool includePrivate) {
  OpDefinitions list;

  for (auto &op : OpManager::getInstance().opMap) {
    for (auto &opVersion : op.second) {
      if (opVersion.second.isPublic || includePrivate) {
        list.insert({opVersion.second.id, opVersion.second.details});
      }
    }
  }

  return list;
}

Attributes OpManager::getAttributesFromAnyMap(
    std::map<std::string, popart::any> attributes) {
  Attributes attr;
  for (auto attribute : attributes) {
    const std::type_info &tinfo = attribute.second.type();
    if (tinfo == typeid(Attributes::Int)) {
      auto value = popart::any_cast<Attributes::Int>(attribute.second);
      attr.setAttribute(attribute.first, value);
    } else if (tinfo == typeid(Attributes::Ints)) {
      auto value = popart::any_cast<Attributes::Ints>(attribute.second);
      attr.setAttribute(attribute.first, value);
    } else if (tinfo == typeid(std::string)) {
      auto value = popart::any_cast<std::string>(attribute.second);
      attr.setAttribute(attribute.first, value);
    } else {
      throw error("Unsupported attribute value type {}", tinfo.name());
    }
  }
  return attr;
}

std::unique_ptr<Op> OpManager::createOp(const OpDomain &opDomain,
                                        const OpType &type,
                                        const int opsetVersion,
                                        Graph &graph,
                                        const std::string &name,
                                        const Scope &scope,
                                        const Attributes &attr,
                                        const std::vector<TensorId> &inputIds) {

  OpManager &self = getInstance();

  OpDomain domain = opDomain;
  if (domain == "")
    domain = Domain::ai_onnx;

  int version    = 0;
  OpInfo *opInfo = nullptr;

  // First find the domain/type
  auto it2 = self.opMap.find(std::make_pair(domain, type));
  if (it2 != self.opMap.end()) {
    for (auto &it3 : it2->second) {

      // Then find the op with the largest version that is less than the opset
      // version
      if (it3.first >= version && it3.first <= opsetVersion) {
        version = it3.first;
        opInfo  = &it3.second;
      }
    }
  }

  if (opInfo != nullptr) {
    return self.create(
        opInfo->id, graph, name, scope, attr, inputIds, opInfo->f1);
  }
  return nullptr;
}

std::unique_ptr<Op> OpManager::createOp(const OperatorIdentifier &opid,
                                        Graph &graph,
                                        const std::string &name,
                                        const Attributes &attr) {

  OpManager &self = getInstance();

  // First find the domain/type
  const auto &it2 = self.opMap.find(std::make_pair(opid.domain, opid.type));

  if (it2 != self.opMap.end()) {
    // Then find the version
    const auto &it3 = it2->second.find(opid.version);

    if (it3 != it2->second.end()) {
      return self.create(opid, graph, name, {}, attr, {}, it3->second.f1);
    }
  }
  return nullptr;
}

std::unique_ptr<Op> OpManager::create(const OperatorIdentifier &opid,
                                      Graph &graph,
                                      const std::string &name,
                                      const Scope &scope,
                                      const Attributes &attr,
                                      const std::vector<TensorId> &inputIds,
                                      OpFactoryFunc func) {

  Op::Settings settings(graph, name, scope);
  settings.setFromAttributes(attr);

  OpCreatorInfo info(opid, settings, attr, inputIds);
  return func(info);
}

OpVersion OpManager::getOpVersionFromOpSet(const OpDomain &opDomain,
                                           const OpType &type,
                                           const int opsetVersion) {
  OpManager &self = getInstance();

  OpDomain domain = opDomain;
  if (domain == "")
    domain = Domain::ai_onnx;

  int version = 0;

  // First find the domain/type
  auto it2 = self.opMap.find(std::make_pair(domain, type));
  if (it2 != self.opMap.end()) {
    for (auto &it3 : it2->second) {

      // Then find the op with the largest version that is less than the opset
      // version
      if (it3.first > version && it3.first <= opsetVersion) {
        version = it3.first;
      }
    }
  }

  return version;
}

std::ostream &operator<<(std::ostream &os,
                         const std::vector<DataType> &dataTypes) {

  for (auto &dt : dataTypes) {

    if (dt != dataTypes[0]) {
      os << ", ";
    }

    os << "tensor(" << dt << ")";
  }

  return os;
}

} // namespace popart
