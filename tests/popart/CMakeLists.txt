find_package(Boost REQUIRED unit_test_framework filesystem)

# Workaround for this bug: https://github.com/boostorg/system/issues/26
# on certain systems/compilers (e.g. compiling the host-runtime-view
# on Centos 7.6 with GCC 7.3).
# TODO: When CMake 3.12 is required use add_compile_definitions() instead.
add_definitions(-DBOOST_ERROR_CODE_HEADER_ONLY)

set(DEFAULT_TEST_VARIANTS "Cpu" CACHE STRING
    "The device variants to run tests on when no variants are set explicitly")

# Generate tests for these devices and variants. Cmake variable ENABLED_TEST_VARIANTS can
# be set with -DPOPART_CMAKE_ARGS=-DENABLED_TEST_VARIANTS=Hw for Hw only tests, for example.
# -DPOPART_CMAKE_ARGS="-DENABLED_TEST_VARIANTS=Cpu$<SEMICOLON>IpuModel$<SEMICOLON>Hw"
# Will enable all tests - note that you need to use '$<SEMICOLON>' not ';'
set(ENABLED_TEST_VARIANTS "Cpu;IpuModel" CACHE STRING "Default (non-hw) test variants")

# Remove any requested variants not present in ${ENABLED_TEST_VARIANTS}
# If no variants were requested filtered ${DEFAULT_TEST_VARIANTS} are returned.
function(sanitise_variants variant_name)
  set(variants ${${variant_name}})
  set(old_variants "${variants}")
  if (NOT old_variants)
    set(old_variants ${DEFAULT_TEST_VARIANTS})
  endif()
  set(new_variants "")
  # Only allow each variant if it is enabled:
  foreach(variant ${old_variants})
    if (${variant} IN_LIST ENABLED_TEST_VARIANTS)
      list(APPEND new_variants ${variant})
    endif()
  endforeach()
  list(REMOVE_DUPLICATES new_variants)
  set(${variant_name} "${new_variants}" PARENT_SCOPE)
endfunction()

# A VARIANT is a colon separated pair "target:[runconfig]". This function
# extracts the "target" part storing it in the second argument.
function(extract_target variant target)
  string(REPLACE ":" ";" vspec ${${variant}})
  list(GET vspec 0 HEAD)
  set(${target} ${HEAD} PARENT_SCOPE)
endfunction()

# A VARIANT is a colon separated pair "target:[runconfig]". This function
# extracts the "runconfig" part storing it in the second argument.
function(extract_runconfig variant run_config)
  string(REPLACE ":" ";" vspec ${${variant}})
  list(LENGTH vspec vlen)
  if (${vlen} EQUAL "1")
    set(${run_config} "default" PARENT_SCOPE)
  else()
    list(GET vspec 1 config)
    set(${run_config} ${config} PARENT_SCOPE)
  endif()
endfunction()

# A VARIANT is a colon separated pair "target:[runconfig]". This function
# extracts a list containing just the "target" parts. Where targets are listed
# with multiple runconfigs in the original list the duplicates are removed
# from the returned list.
function(extract_targets variants targets)
  set(extracted_targets "")
  foreach(variant ${${variants}})
    string(REPLACE ":" ";" vspec ${variant})
    list(GET vspec 0 HEAD)
    list(APPEND extracted_targets ${HEAD})
  endforeach()
  list(REMOVE_DUPLICATES extracted_targets)
  set(${targets} ${extracted_targets} PARENT_SCOPE)
endfunction()

function(add_test_executable name)
    add_executable(${name} ${ARGN})
  target_include_directories(${name}
      PRIVATE
      ${CMAKE_CURRENT_SOURCE_DIR}
      $<TARGET_PROPERTY:popart,INCLUDE_DIRECTORIES>)
  target_link_libraries(${name}
    popart
    ${CMAKE_THREAD_LIBS_INIT}
    ${Boost_LIBRARIES}
  )
  if (NOT Boost_UNIT_TEST_FRAMEWORK_LIBRARY MATCHES "\\.a$")
    target_compile_definitions(${name} PRIVATE -DBOOST_TEST_DYN_LINK)
  endif()

  set_property(TARGET ${name}
              APPEND_STRING PROPERTY
              COMPILE_FLAGS "-DTEST_TARGET=TestDeviceType::${TEST_TARGET}")
endfunction()

# Optional arguments:
# VARIANTS
# Run with the specified VARIANTS (or all enabled VARIANTS if
# none are specified). The first parameter is the test name, the following are
# source files, and finally everything after VARIANTS are variant
# specifications.
#
# Mark the test as requiring two IPUs - the relevant fixture will be added
#
# Currently supported targets are "Cpu;IpuModel;Sim;Hw"
#
#
# LABELS
#
# Semicolon separated list of labels to be set for this test
#
#
# DUAL
# A variation specification must be of the form "target:[config]"
#
function(add_popart_cpp_unit_test name)
  set(oneValueArgs "VARIANTS;LABELS")
  cmake_parse_arguments(test_args "" "FILES" "${oneValueArgs}" "FILES;${ARGN}")
  set(FILES "${test_args_FILES}")

  sanitise_variants(test_args_VARIANTS)
  extract_targets(test_args_VARIANTS TARGETS)
  message(STATUS
    "Adding cpp test '${name}' with variants: ${test_args_VARIANTS}")

  foreach(TEST_TARGET ${TARGETS})
    set(executable_name "${TEST_TARGET}_${name}")
    add_test_executable(${executable_name} ${FILES})
  endforeach()

  foreach(VARIANT ${test_args_VARIANTS})
    extract_target(VARIANT TEST_TARGET)
    extract_runconfig(VARIANT TEST_CONFIG)

    set(test_name "${TEST_TARGET}_${TEST_CONFIG}_${name}")
    set(executable_name "${TEST_TARGET}_${name}")
    add_test(NAME "${test_name}"
      COMMAND ${executable_name}
      WORKING_DIRECTORY ${CMAKE_CURRENT_BUILD_DIR})

    set(test_env ${TEST_ENVIRONMENT})
    if (${TEST_CONFIG} STREQUAL "cpp")
      list(APPEND test_env "IPU_POPLAR_RT_GP_SUFFIX=_c")
    endif()

    set_tests_properties(${test_name} PROPERTIES
                         ENVIRONMENT "${test_env}")
    set_tests_properties(${test_name} PROPERTIES
                          LABELS "${test_args_LABELS}")
    if (${TEST_TARGET} STREQUAL "Hw")
      # Make sure tests that use physical IPUs only run if an appropriate
      # number were available according to the relevant test fixture:
      set_tests_properties(${test_name}
        PROPERTIES FIXTURES_REQUIRED SingleIpuIsAvailable)
    endif()
  endforeach()
endfunction()

add_popart_cpp_unit_test(aliaszerocopytest aliaszerocopy_test.cpp)
add_popart_cpp_unit_test(allocatortest allocator_test.cpp)
add_popart_cpp_unit_test(buildertest builder_test.cpp)
add_popart_cpp_unit_test(builderpartialstest builder_partials_test.cpp)
add_popart_cpp_unit_test(collectivestest collectives_test.cpp VARIANTS "Hw")
add_popart_cpp_unit_test(custompatterntest custom_pattern_test.cpp)
add_popart_cpp_unit_test(dataflowtest dataflowtest.cpp)
add_popart_cpp_unit_test(decomposegradientsummationtest decompose_gradient_summation_test.cpp)
add_popart_cpp_unit_test(exceptiontest exceptiontest.cpp)
add_popart_cpp_unit_test(inputshapeinfotest inputshapeinfotest.cpp)
add_popart_cpp_unit_test(irhashtest ir_hash_test.cpp VARIANTS "IpuModel")
add_popart_cpp_unit_test(isnonlinearitytest is_nonlinearity_test.cpp)
add_popart_cpp_unit_test(isnormtest is_norm_test.cpp)
add_popart_cpp_unit_test(loggingtest loggingtest.cpp)
add_popart_cpp_unit_test(maxcliquetest maxclique_test.cpp)
add_popart_cpp_unit_test(mergecopiestest mergecopies_test.cpp)
add_popart_cpp_unit_test(nogradoptest no_gradop_test.cpp)
add_popart_cpp_unit_test(numpybroadcastshapetest numpybroadcastshapetest.cpp)
add_popart_cpp_unit_test(opmanagertest op_manager_test.cpp)
add_popart_cpp_unit_test(prunetest prune_test.cpp)
add_popart_cpp_unit_test(syncpatterntest sync_pattern_test.cpp VARIANTS "Hw")
add_popart_cpp_unit_test(syntheticdatatest synthetic_data_test.cpp)
add_popart_cpp_unit_test(transformtest transform_test.cpp)
add_popart_cpp_unit_test(vertex_vgid_test vertex_vgid_test.cpp VARIANTS "IpuModel")
add_popart_cpp_unit_test(viewchangingtest view_changing_test.cpp)

# Add a test that targets c++11 to check that the popart interface is c++11.
# If the interface is not c++11, the build should fail.
add_test_executable(verify_cxx_11_interface verify_cxx_11_interface.cpp)
set_property(TARGET "verify_cxx_11_interface" PROPERTY CXX_STANDARD 11)

# Python unit test function
# Uses VARIANTS as above for add_popart_cpp_unit_test
function(add_popart_py_unit_test name)
  set(oneValueArgs "VARIANTS;LABELS")
  cmake_parse_arguments(test_args "" "FILES" "${oneValueArgs}" "FILES;${ARGN}")
  set(FILES "${test_args_FILES}")

  sanitise_variants(test_args_VARIANTS)
  extract_targets(test_args_VARIANTS TARGETS)
  message(STATUS
    "Adding python test '${name}' with variants: ${test_args_VARIANTS}")

  foreach(VARIANT ${test_args_VARIANTS})
    extract_target(VARIANT TEST_TARGET)
    extract_runconfig(VARIANT TEST_CONFIG)

    set(test_name "${TEST_TARGET}_${TEST_CONFIG}_${name}")
    add_test(NAME "${test_name}"
           COMMAND ${Python3_EXECUTABLE} -m pytest -s ${CMAKE_CURRENT_SOURCE_DIR}/${name}.py
           WORKING_DIRECTORY ${CMAKE_CURRENT_BUILD_DIR})
    set_tests_properties(${test_name} PROPERTIES
                         ENVIRONMENT TEST_TARGET=${TEST_TARGET})
  endforeach()
endfunction()

# add_popart_py_unit_test("test_util") Utility
# add_popart_py_unit_test("test_session") Utility

# Python tests. Please try to sort alphabetically.
add_popart_py_unit_test(addbias)
add_popart_py_unit_test(all_constexpr)
add_popart_py_unit_test(anchorreturntype_test VARIANTS Hw)
add_popart_py_unit_test(annotations_test)
add_popart_py_unit_test(auto_virtual_graph_test VARIANTS IpuModel)
add_popart_py_unit_test(builder_name_test)
add_popart_py_unit_test(builder_test)
add_popart_py_unit_test(collectives_test VARIANTS Hw)
add_popart_py_unit_test(context_scope_test)
add_popart_py_unit_test(convolution_options_test VARIANTS IpuModel)
add_popart_py_unit_test(cycle_count_test VARIANTS Hw)
add_popart_py_unit_test(decompose_gradient_summation_test VARIANTS IpuModel)
add_popart_py_unit_test(device_test VARIANTS Hw)
add_popart_py_unit_test(dim_param_test)
add_popart_py_unit_test(dropout_replicated_pipeline VARIANTS Hw)
add_popart_py_unit_test(exception_test VARIANTS Hw)
add_popart_py_unit_test(export_test)
add_popart_py_unit_test(float_to_half_conversion_test)
add_popart_py_unit_test(fp16_test)
add_popart_py_unit_test(gradient_accumulation_test VARIANTS IpuModel)
add_popart_py_unit_test(graph_caching_test VARIANTS IpuModel)
add_popart_py_unit_test(graph_replication_test VARIANTS Hw)
add_popart_py_unit_test(import_test)
add_popart_py_unit_test(ipu_copy_test VARIANTS IpuModel)
add_popart_py_unit_test(ipu_gather_test)
add_popart_py_unit_test(loader_test)
add_popart_py_unit_test(loss_scaling_test)
add_popart_py_unit_test(mapping_test VARIANTS IpuModel)
add_popart_py_unit_test(memory_regression_test VARIANTS IpuModel)
add_popart_py_unit_test(net_test)
add_popart_py_unit_test(offline_compilation)
add_popart_py_unit_test(optimizer_test)
add_popart_py_unit_test(options_test)
add_popart_py_unit_test(outlining_test)
add_popart_py_unit_test(padsumpattern)
add_popart_py_unit_test(partials_tests VARIANTS IpuModel)
add_popart_py_unit_test(patterns_test)
add_popart_py_unit_test(prefetch_test VARIANTS Hw)
add_popart_py_unit_test(printtensor)
add_popart_py_unit_test(prune_all_error_test)
add_popart_py_unit_test(random_test VARIANTS Hw)
add_popart_py_unit_test(recompute_compatibility_test)
add_popart_py_unit_test(report_test VARIANTS IpuModel)
add_popart_py_unit_test(report_test_cpu)
add_popart_py_unit_test(saved_executable VARIANTS Hw)
add_popart_py_unit_test(saved_executable_cpu)
#Temporarily disabled, see T22702.
#add_popart_py_unit_test(serialise_matmul_manually)
add_popart_py_unit_test(stream_test)
add_popart_py_unit_test(synthetic_data_test VARIANTS Hw)
add_popart_py_unit_test(test_groupHostSync VARIANTS IpuModel)
add_popart_py_unit_test(train_then_infer_test VARIANTS IpuModel)
add_popart_py_unit_test(variable_inference_test)
add_popart_py_unit_test(virtual_graph_check_test)
add_popart_py_unit_test(virtual_graph_test VARIANTS IpuModel)
add_popart_py_unit_test(type_casting_test)

# Test to check for boost in popart headers.
set(POPART_INCLUDE_DIR ${PROJECT_SOURCE_DIR}/willow/include)
add_test(NAME "boost_free_interface_test"
  COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/boost_free_interface_test.py ${POPART_INCLUDE_DIR})

# device_test should be run serially as it tests that it can attach to each
# IPU available on the machine it is run on.
if ("Hw" IN_LIST ENABLED_TEST_VARIANTS)
  set_tests_properties("Hw_default_device_test" PROPERTIES RUN_SERIAL TRUE)
endif()

add_subdirectory(anchor_tests)
add_subdirectory(auto_virtual_graph_tests)
add_subdirectory(codelet_tests)
add_subdirectory(constexpr_tests)
add_subdirectory(dot_tests)
add_subdirectory(dropout_tests)
add_subdirectory(inplace_tests)
add_subdirectory(logical_if_tests)
add_subdirectory(matmul_tests)
add_subdirectory(operators_test)
add_subdirectory(optimizer_tests)
add_subdirectory(pattern_tests)
add_subdirectory(pingpong_tests)
add_subdirectory(pipelining_tests)
add_subdirectory(recompute_tests)
add_subdirectory(scheduling_tests)
add_subdirectory(session_api_tests)
add_subdirectory(stepio_tests)
add_subdirectory(subgraph_tests)
add_subdirectory(tensor_tests)
add_subdirectory(topk_tests)
add_subdirectory(transformation_tests)
