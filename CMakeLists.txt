cmake_minimum_required(VERSION 3.23)

if(NOT DEFINED CMAKE_OSX_DEPLOYMENT_TARGET)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "10.15" CACHE STRING
        "Minimum macOS deployment target")
endif()

project(fmi_library VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(FMI_BUILD_EXAMPLES "Build the FMI client examples" ON)
option(BUILD_TESTING "Build the FMI model contract tests" OFF)
include(CTest)

set(FMI_PLATFORM_TAG "" CACHE STRING
    "Platform tag used for output directories and runtime staging")
set(FMI_ONNXRUNTIME_ROOT "${PROJECT_SOURCE_DIR}/third_party/onnxruntime" CACHE PATH
    "Directory containing the fetched ONNX Runtime headers")

# * Resolves <os>-<arch>, for example win64-x64, linux64-aarch64, darwin64-arm64.
function(fmi_detect_platform_tag output_variable)
    if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
        set(fmi_platform_os "win64")
    elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        set(fmi_platform_os "linux64")
    elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
        set(fmi_platform_os "darwin64")
    else()
        message(FATAL_ERROR "Unsupported target system '${CMAKE_SYSTEM_NAME}'.")
    endif()

    set(fmi_architectures "${CMAKE_OSX_ARCHITECTURES}")
    if(NOT fmi_architectures)
        set(fmi_architectures "${CMAKE_SYSTEM_PROCESSOR}")
    endif()
    if(NOT fmi_architectures)
        set(fmi_architectures "${CMAKE_HOST_SYSTEM_PROCESSOR}")
    endif()
    string(TOLOWER "${fmi_architectures}" fmi_architectures)

    set(fmi_platform_arch "")
    if(fmi_architectures MATCHES "arm64|aarch64")
        if(fmi_platform_os STREQUAL "linux64")
            set(fmi_platform_arch "aarch64")
        else()
            set(fmi_platform_arch "arm64")
        endif()
    endif()
    if(fmi_architectures MATCHES "x86_64|amd64|x64")
        set(fmi_platform_arch "x64")
    endif()
    if(fmi_architectures MATCHES "arm64|aarch64" AND
        fmi_architectures MATCHES "x86_64|amd64|x64")
        set(fmi_platform_arch "universal2")
    endif()

    if(NOT fmi_platform_arch)
        message(FATAL_ERROR
            "Unsupported target processor '${fmi_architectures}'. "
            "Set FMI_PLATFORM_TAG explicitly if this target is supported.")
    endif()

    set(${output_variable} "${fmi_platform_os}-${fmi_platform_arch}" PARENT_SCOPE)
endfunction()

if(NOT FMI_PLATFORM_TAG)
    fmi_detect_platform_tag(fmi_detected_platform_tag)
    set(FMI_PLATFORM_TAG "${fmi_detected_platform_tag}" CACHE STRING
        "Platform tag used for output directories and runtime staging" FORCE)
endif()

set(FMI_LIBRARY_OUTPUT_DIR "${CMAKE_BINARY_DIR}/bin/lib/${FMI_PLATFORM_TAG}")
set(FMI_EXECUTABLE_OUTPUT_DIR "${CMAKE_BINARY_DIR}/bin/exe/${FMI_PLATFORM_TAG}")
set(FMI_RESOURCE_OUTPUT_DIR "${CMAKE_BINARY_DIR}/bin/resources")

function(fmi_configure_target target_name)
    target_compile_features(${target_name} PRIVATE cxx_std_17)
    if(NOT MSVC)
        target_compile_options(${target_name} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    set_target_properties(${target_name} PROPERTIES
        CXX_VISIBILITY_PRESET hidden
        VISIBILITY_INLINES_HIDDEN ON
    )
endfunction()

# * Refresh Windows DLLs even when their import libraries do not change.
function(fmi_stage_model_libraries target_name)
    if(NOT WIN32)
        return()
    endif()
    set(runtime_libraries "")
    foreach(model_target IN LISTS ARGN)
        list(APPEND runtime_libraries "$<TARGET_FILE:${model_target}>")
    endforeach()
    add_custom_target(${target_name}_runtime
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:${target_name}>"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${runtime_libraries} "$<TARGET_FILE_DIR:${target_name}>"
        DEPENDS ${ARGN}
        VERBATIM
    )
    add_dependencies(${target_name} ${target_name}_runtime)
endfunction()

# * Shared FMU library. CORE is required; remaining arguments are sources.
function(fmi_add_model target_name)
    cmake_parse_arguments(PARSE_ARGV 1 FMI_MODEL "" "CORE" "")
    if(NOT DEFINED FMI_MODEL_CORE OR FMI_MODEL_CORE STREQUAL "")
        message(FATAL_ERROR
            "fmi_add_model(${target_name}): CORE is required. "
            "Specify CORE fmi2_core or CORE fmi3_core.")
    endif()
    if(NOT TARGET "${FMI_MODEL_CORE}")
        message(FATAL_ERROR
            "fmi_add_model(${target_name}): CORE target '${FMI_MODEL_CORE}' "
            "does not exist.")
    endif()
    get_target_property(fmi_model_core_type "${FMI_MODEL_CORE}" TYPE)
    if(NOT fmi_model_core_type STREQUAL "OBJECT_LIBRARY")
        message(FATAL_ERROR
            "fmi_add_model(${target_name}): CORE target '${FMI_MODEL_CORE}' "
            "must be an OBJECT library.")
    endif()
    add_library(${target_name} SHARED ${FMI_MODEL_UNPARSED_ARGUMENTS}
        $<TARGET_OBJECTS:${FMI_MODEL_CORE}>)
    target_link_libraries(${target_name} PRIVATE ${CMAKE_DL_LIBS})
    fmi_configure_target(${target_name})
    # * Keep modelIdentifier as the basename on every platform.
    set_target_properties(${target_name} PROPERTIES
        PREFIX ""
        IMPORT_PREFIX ""
        RUNTIME_OUTPUT_DIRECTORY "${FMI_LIBRARY_OUTPUT_DIR}"
        LIBRARY_OUTPUT_DIRECTORY "${FMI_LIBRARY_OUTPUT_DIR}"
        ARCHIVE_OUTPUT_DIRECTORY "${FMI_LIBRARY_OUTPUT_DIR}"
    )
    if(MINGW)
        target_compile_options(${target_name} PRIVATE -Wno-cast-function-type)
        target_link_options(${target_name} PRIVATE
            -static-libgcc
            -static-libstdc++
        )
    endif()
    if(MSVC)
        target_compile_definitions(${target_name} PRIVATE _CRT_SECURE_NO_WARNINGS)
    endif()
endfunction()

if(NOT EXISTS "${FMI_ONNXRUNTIME_ROOT}/include/onnxruntime_c_api.h")
    message(FATAL_ERROR
        "ONNX Runtime headers are missing from\n"
        "  ${FMI_ONNXRUNTIME_ROOT}/include\n"
        "Fetch them with:\n"
        "  python third_party/onnxruntime/fetch.py")
endif()

include_directories("${PROJECT_SOURCE_DIR}/include")
include_directories(SYSTEM "${FMI_ONNXRUNTIME_ROOT}/include")

add_subdirectory(src)

if(FMI_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
