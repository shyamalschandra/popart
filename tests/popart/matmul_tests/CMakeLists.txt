# Tests of ONNX matmul

add_popart_cpp_unit_test(basic0_matmul_test 
                         basic0_matmul_test.cpp)

add_popart_cpp_unit_test(dataless0_train_matmul_test 
                          dataless0_train_matmul_test.cpp)

# TODO: T18027 - Re-enable this test once its assumptions are true again.
add_popart_cpp_unit_test(matmulgradpattern_test
                         matmulgradpattern_test.cpp)
set_tests_properties(Cpu_default_matmulgradpattern_test PROPERTIES WILL_FAIL TRUE)

add_popart_py_unit_test(matmul_partials_tests VARIANTS IpuModel)
