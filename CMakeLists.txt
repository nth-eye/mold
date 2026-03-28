cmake_minimum_required(VERSION 3.20.0 FATAL_ERROR)
project(mold VERSION 0.1.0 LANGUAGES CXX)

add_library(mold INTERFACE)
target_include_directories(mold INTERFACE include)
target_compile_features(mold INTERFACE cxx_std_20)
target_compile_options(mold INTERFACE 
    "-Wall" 
    "-Wextra" 
    "-Wpedantic"
    # "-fdata-sections"
    # "-ffunction-sections"
    # "-fvisibility=hidden"
)

if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
    add_executable(exec_mold main.cpp)
    target_link_libraries(exec_mold PRIVATE mold)
    target_compile_features(exec_mold PRIVATE cxx_std_20)
    target_compile_definitions(exec_mold PRIVATE MOLD_REFLECTION_CBOR_FIELD_KEYS=true)
    target_compile_options(exec_mold PRIVATE
        "-Wall"
        "-Wextra"
        "-Wpedantic"
    )

    add_executable(test_mold
        test/test_main.cpp
        test/test_json.cpp
        test/test_cbor.cpp
        test/test_msgpack.cpp
        test/test_types.cpp
        test/test_util.cpp
        test/test_vector.cpp
    )
    target_include_directories(test_mold PRIVATE test)
    target_link_libraries(test_mold PRIVATE mold)
    target_compile_features(test_mold PRIVATE cxx_std_20)
    target_compile_definitions(test_mold PRIVATE MOLD_REFLECTION_CBOR_FIELD_KEYS=true)
    target_compile_options(test_mold PRIVATE
        "-Wall"
        "-Wextra"
        "-Wpedantic"
    )
endif()