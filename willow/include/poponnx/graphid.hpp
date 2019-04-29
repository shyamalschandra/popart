#ifndef GUARD_NEURALNET_GRAPHID_HPP
#define GUARD_NEURALNET_GRAPHID_HPP
#include <string>

namespace poponnx {

class GraphId {
public:
  GraphId() = delete;
  GraphId(const std::string &);

  bool operator<(const GraphId &) const;

  static const GraphId &root();

  std::string str() const;

private:
  std::string id;
};

std::ostream &operator<<(std::ostream &, const GraphId &);

} // namespace poponnx

#endif