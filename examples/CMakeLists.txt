# * Console driver for one model. Sources follow the model target name.
function(fmi_add_example target_name model_target)
    add_executable(${target_name} ${ARGN})
    target_link_libraries(${target_name} PRIVATE ${model_target})
    fmi_configure_target(${target_name})
    set_target_properties(${target_name} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${FMI_EXECUTABLE_OUTPUT_DIR}"
        ARCHIVE_OUTPUT_DIRECTORY "${FMI_EXECUTABLE_OUTPUT_DIR}"
    )

    if(MINGW)
        target_link_options(${target_name} PRIVATE
            -static-libgcc
            -static-libstdc++
        )
    endif()
    fmi_stage_model_libraries(${target_name} ${model_target})

    string(REGEX REPLACE "^fmi[23]_cs_" "" fmi_model_name "${model_target}")
    set(fmi_resource_directory
        "${FMI_RESOURCE_OUTPUT_DIR}/model/${fmi_model_name}/${FMI_PLATFORM_TAG}")
    file(TO_CMAKE_PATH "${fmi_resource_directory}" fmi_resource_directory)
    target_compile_definitions(${target_name} PRIVATE
        "FMI_EXAMPLE_RESOURCE_DIRECTORY=\"${fmi_resource_directory}\""
    )

    if(APPLE)
        set_property(TARGET ${target_name} APPEND PROPERTY BUILD_RPATH
            "@loader_path/../../lib/${FMI_PLATFORM_TAG}")
    elseif(UNIX)
        set_property(TARGET ${target_name} APPEND PROPERTY BUILD_RPATH
            "$ORIGIN/../../lib/${FMI_PLATFORM_TAG}")
    endif()
endfunction()

# * Console driver for one onnxengine predictor. Sources follow the target name.
# * The predictor dlopens ONNX Runtime from the resource directory, so nothing
# * is linked against it and no rpath is needed.
function(fmi_add_engine_example target_name resource_name)
    add_executable(${target_name} ${ARGN})
    target_link_libraries(${target_name} PRIVATE ${CMAKE_DL_LIBS})
    fmi_configure_target(${target_name})
    set_target_properties(${target_name} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${FMI_EXECUTABLE_OUTPUT_DIR}"
        ARCHIVE_OUTPUT_DIRECTORY "${FMI_EXECUTABLE_OUTPUT_DIR}"
    )

    if(MINGW)
        target_compile_options(${target_name} PRIVATE -Wno-cast-function-type)
        target_link_options(${target_name} PRIVATE
            -static-libgcc
            -static-libstdc++
        )
    endif()

    set(fmi_resource_directory
        "${FMI_RESOURCE_OUTPUT_DIR}/onnxengine/${resource_name}/${FMI_PLATFORM_TAG}")
    file(TO_CMAKE_PATH "${fmi_resource_directory}" fmi_resource_directory)
    target_compile_definitions(${target_name} PRIVATE
        "ONNXENGINE_EXAMPLE_RESOURCE_DIRECTORY=\"${fmi_resource_directory}\""
    )
endfunction()

add_subdirectory(model)
add_subdirectory(onnxengine)
