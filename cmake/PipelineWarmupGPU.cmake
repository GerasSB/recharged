include_guard(GLOBAL)
if(NOT BUILD_TESTING OR NOT TARGET aurora_gx)
    return()
endif()
add_executable(pipeline_warmup_gpu_tests tests/pipeline_warmup_gpu.cpp)
add_dependencies(pipeline_warmup_gpu_tests verify_prepared)
target_include_directories(pipeline_warmup_gpu_tests PRIVATE
    "${MSCHARGED_AURORA_PREPARED}/lib")
target_compile_features(pipeline_warmup_gpu_tests PRIVATE cxx_std_20)
target_link_libraries(pipeline_warmup_gpu_tests PRIVATE
    aurora::gx aurora::os aurora::vi aurora::core Threads::Threads sqlite3 absl::flat_hash_map)
if(MSCHARGED_TEST_VULKAN)
    add_test(NAME pipeline_warmup_gpu COMMAND pipeline_warmup_gpu_tests --backend vulkan)
    set_tests_properties(pipeline_warmup_gpu PROPERTIES TIMEOUT 180
        LABELS "gpu;vulkan" RESOURCE_LOCK gx_check
        ENVIRONMENT "VK_INSTANCE_LAYERS=VK_LAYER_KHRONOS_validation"
        FAIL_REGULAR_EXPRESSION "VUID-|Error:|Validation Error|Pipeline warmup GPU failure")
endif()
