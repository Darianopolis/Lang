cmake_minimum_required(VERSION 4.0.0)
project(lang)

include(FetchContent)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

function(set_default_compile_options target)
    target_compile_options(${target} PUBLIC
        -Wno-missing-field-initializers
        -Wno-comment
        -Wno-binding-in-condition
        -Wno-deprecated-declarations
        )
    target_compile_features(${target} PUBLIC cxx_std_26)
endfunction()

# simdutf

FetchContent_Declare(simdutf
    GIT_REPOSITORY "https://github.com/simdutf/simdutf.git"
    GIT_TAG "master")
set(SIMDUTF_TESTS OFF)
set(SIMDUTF_TOOLS OFF)
FetchContent_MakeAvailable(simdutf)

# backward-cpp

FetchContent_Declare(backward_cpp
    GIT_REPOSITORY "https://github.com/bombela/backward-cpp.git"
    GIT_TAG "master")
FetchContent_MakeAvailable(backward_cpp)

# add_library(backward-cpp-test)
# target_sources(backward-cpp-test PUBLIC ${backward_cpp_SOURCE_DIR}/backward.cpp)
# target_include_directories(backward-cpp-test PUBLIC ${backward_cpp_SOURCE_DIR})

add_library(test_backward_interface INTERFACE)
set_target_properties(test_backward_interface PROPERTIES EXPORT_NAME Interface)
target_compile_definitions(test_backward_interface INTERFACE ${BACKWARD_DEFINITIONS})
target_include_directories(test_backward_interface INTERFACE ${BACKWARD_INCLUDE_DIRS})
if(BACKWARD_HAS_EXTERNAL_LIBRARIES)
    target_link_libraries(test_backward_interface INTERFACE ${BACKWARD_LIBRARIES})
endif()

add_library(test_backward_object OBJECT ${backward_cpp_SOURCE_DIR}/backward.cpp)
set_target_properties(test_backward_object PROPERTIES EXPORT_NAME Object)
target_link_libraries(test_backward_object PUBLIC test_backward_interface)

# lang

add_executable(lang)
set_default_compile_options(lang)
target_include_directories(lang PUBLIC src)
target_sources(lang PUBLIC
    src/main.cpp
    src/lang/scan.cpp
    src/lang/parse.cpp
    )
target_link_libraries(lang PUBLIC stdc++exp)
target_link_libraries(lang PUBLIC simdutf::simdutf)
# target_link_libraries(lang PUBLIC Backward::Object)
target_link_libraries(lang PUBLIC test_backward_object)
