#include <boost/filesystem.hpp>
#include <fstream>
#include <sstream>
#include <vector>
#include <poponnx/error.hpp>
#include <poponnx/filereader.hpp>
#include <poponnx/names.hpp>

namespace poponnx {
namespace io {

std::string getCanonicalDirName(const std::string &dirName0) {
  namespace bf = boost::filesystem;
  if (!bf::is_directory(dirName0)) {
    std::stringstream ss;
    ss << "Directory does not exist: " << dirName0;
    throw poponnx::error(ss.str());
  }
  bf::path p(dirName0);
  return bf::canonical(dirName0).string();
}

std::string getCanonicalFilename(const std::string &fn) {
  namespace bf = boost::filesystem;
  bf::path p(fn);
  return bf::canonical(fn).string();
}

std::string appendDirFn(const std::string &dir, const std::string &fn) {
  boost::filesystem::path p(dir);
  auto fullPath = p / fn;
  return fullPath.string();
}

bool isRegularFile(const std::string &filename) {
  boost::system::error_code ec;
  auto isRegularFile = boost::filesystem::is_regular_file(filename, ec);
  // If the file system API reports an error then we assume that this is not a
  // regular file.
  // See
  // https://www.boost.org/doc/libs/1_45_0/libs/filesystem/v3/doc/reference.html#status
  return ec ? false : isRegularFile;
}

void confirmRegularFile(const std::string &filename) {
  if (!boost::filesystem::is_regular_file(filename)) {
    std::stringstream ss;
    ss << filename << " is not a regular file, cannot load";
    throw error(ss.str());
  }
}

OnnxTensors getInputTensors(const onnx::GraphProto &g, const std::string &dir) {
  auto fns = getMatchFns(dir, "input");
  std::vector<std::string> names;
  for (auto &x : g.input()) {
    names.push_back(x.name());
  }
  return getAndMatchTensors(fns, names);
}

OnnxTensors getOutputTensors(const onnx::GraphProto &g,
                             const std::string &dir) {
  auto fns = getMatchFns(dir, "output");
  std::vector<std::string> names;
  for (auto &x : g.output()) {
    names.push_back(x.name());
  }
  return getAndMatchTensors(fns, names);
}

static bool getModelFromStream(std::istream &istream,
                               onnx::ModelProto &modelProto) {
  return modelProto.ParseFromIstream(&istream);
}

onnx::ModelProto getModelFromFile(const std::string &filename) {
  // Verify that the version of the library that we linked against is
  // compatible with the version of the headers we compiled against.
  // As suggested at developers.google.com/protocol-buffers/docs/cpptutorial
  GOOGLE_PROTOBUF_VERIFY_VERSION;

  confirmRegularFile(filename);
  std::fstream input(filename, std::ios::in | std::ios::binary);

  if (!input.is_open()) {
    std::stringstream ss;
    ss << "Failed to open file " << filename;
    throw error(ss.str());
  }

  onnx::ModelProto modelProto;

  if (!getModelFromStream(input, modelProto)) {
    std::stringstream ss;
    ss << "Failed to parse ModelProto from file " << filename;
    throw error(ss.str());
  }

  return modelProto;
}

onnx::ModelProto getModelFromString(const std::string &stringProto) {
  // Verify that the version of the library that we linked against is
  // compatible with the version of the headers we compiled against.
  // As suggested at developers.google.com/protocol-buffers/docs/cpptutorial
  GOOGLE_PROTOBUF_VERIFY_VERSION;

  std::stringstream input(stringProto);

  onnx::ModelProto modelProto;

  if (!getModelFromStream(input, modelProto)) {
    throw error("Failed to parse ModelProto from string");
  }

  return modelProto;
}

void writeModel(const onnx::ModelProto &model, const std::string &filename) {

  std::ofstream ofs;
  ofs.open(filename, std::ofstream::out | std::ofstream::binary);
  if (!ofs.is_open()) {
    std::stringstream ss;
    ss << "Failed to open file " << filename;
    throw error(ss.str());
  }

  // Standard Message Methods have this functionality for serializing
  // https://developers.google.com/protocol-buffers/docs/cpptutorial
  if (!model.SerializeToOstream(&ofs)) {
    std::stringstream ss;
    ss << "Failed to serialize ModelProto to " << filename;
    throw error(ss.str());
  }
}

onnx::TensorProto getTensor(const std::string &filename) {

  confirmRegularFile(filename);
  std::fstream fs(filename, std::ios::in | std::ios::binary);

  if (!fs.is_open()) {
    std::stringstream ss;
    ss << "failed to open file " << filename;
    throw error(ss.str());
  }

  onnx::TensorProto tensor;
  if (!tensor.ParseFromIstream(&fs)) {
    std::stringstream ss;
    ss << "Failed to parse TensorProto from " << filename;
    throw error(ss.str());
  }

  return tensor;
}

OnnxTensors getAndMatchTensors(const std::vector<std::string> &fns,
                               const std::vector<std::string> &names) {
  namespace bf = boost::filesystem;

  OnnxTensors tensors;
  for (const auto &fn : fns) {
    auto tensor = getTensor(fn);
    // Using the specific naming convention in onnx examples repo
    bf::path p(fn);
    auto name   = p.filename().string();
    auto dStart = name.find('_');
    auto dEnd   = name.find('.');
    auto numStr = name.substr(dStart + 1, dEnd - dStart - 1);
    auto number = std::stoul(numStr);
    if (number >= names.size()) {
      std::stringstream errmss;
      errmss << "number extracted from filename exceeds size of names. "
             << "number = " << number
             << " and size of names = " << names.size();
      throw error(errmss.str());
    }
    // At this point Tensor does not have a name (at least in the test suite).
    tensor.set_name(names[number]);
    auto tensorName = tensor.name();
    tensors.insert({tensorName, std::move(tensor)});
  }
  return tensors;
}

// return all names of full path names of files which match to_match
std::vector<std::string> getMatchFns(const std::string &dir,
                                     const std::string &to_match) {
  namespace bf = boost::filesystem;
  std::vector<std::string> matches;
  auto fns = getFns(dir);
  for (const auto &fn : fns) {
    bf::path p(fn);
    std::string filename = p.filename().string();
    if (filename.find(to_match) != std::string::npos) {
      matches.push_back(fn);
    }
  }
  return matches;
}

template <typename T>
std::vector<std::string> getInDir(const std::string &dir, T check) {
  // std::function<bool(const boost::filesystem::path &path)>
  std::vector<std::string> fns;
  namespace bf = boost::filesystem;
  bf::path p(dir);
  if (!is_directory(p)) {
    std::stringstream ss;
    ss << p << " in not a directory, bailing from getInDir";
    throw error(ss.str());
  } else {
    bf::directory_iterator eod;
    for (bf::directory_iterator dir_itr(p); dir_itr != eod; ++dir_itr) {
      auto bf_path = dir_itr->path();
      if (check(bf_path)) {
        auto fn = bf_path.string();
        fns.push_back(fn);
      }
    }
  }
  return fns;
}

std::vector<std::string> getDirns(const std::string &dir) {
  auto is_dir = [](const boost::filesystem::path &path) {
    return boost::filesystem::is_directory(path);
  };
  return getInDir(dir, is_dir);
}
// return all full path names for regular files in dir
std::vector<std::string> getFns(const std::string &dir) {
  auto is_reg = [](const boost::filesystem::path &path) {
    return boost::filesystem::is_regular_file(path);
  };
  return getInDir(dir, is_reg);
}
} // namespace io
} // namespace poponnx
