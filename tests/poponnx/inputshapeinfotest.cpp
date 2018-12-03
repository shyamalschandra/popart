#define BOOST_TEST_MODULE EarlyUnfoTest

#include <boost/test/unit_test.hpp>

#include <poponnx/error.hpp>
#include <poponnx/inputshapeinfo.hpp>
#include <poponnx/tensorinfo.hpp>

using namespace poponnx;

BOOST_AUTO_TEST_CASE(InputShapeInfo_Case1) {
  auto ei = poponnx::InputShapeInfo();

  BOOST_CHECK(ei.has("cat") == false);
  BOOST_CHECK(ei.getAllTensorIds().size() == 0);
  BOOST_CHECK_THROW(ei.get("cat"), poponnx::error);

  poponnx::TensorInfo input("FLOAT", std::vector<int64_t>({2, 2}));
  ei.add("cat", input);

  BOOST_CHECK(ei.has("cat") == true);
  BOOST_CHECK(ei.getAllTensorIds().size() == 1);
  auto &output = ei.get("cat");

  BOOST_CHECK(input == output);

  poponnx::InputShapeInfo ei2(ei);

  BOOST_CHECK(ei.has("cat") == true);
  BOOST_CHECK(ei2.has("cat") == true);
}