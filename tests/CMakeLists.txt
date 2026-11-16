find_package(Python3 REQUIRED COMPONENTS Interpreter)

foreach(model_name IN ITEMS vector_scaler_onnx window_pure_onnx)
    set(test_target test_cs_${model_name})
    set(resource_directory
        "${FMI_RESOURCE_OUTPUT_DIR}/model/${model_name}/${FMI_PLATFORM_TAG}")

    add_executable(${test_target} cs_contract_test.cpp)
    target_link_libraries(${test_target} PRIVATE
        fmi2_cs_${model_name}
        fmi3_cs_${model_name}
    )
    fmi_configure_target(${test_target})
    if(MINGW)
        target_compile_options(${test_target} PRIVATE -Wno-cast-function-type)
        target_link_options(${test_target} PRIVATE -static-libgcc -static-libstdc++)
    endif()
    fmi_stage_model_libraries(${test_target}
        fmi2_cs_${model_name}
        fmi3_cs_${model_name}
    )

    add_test(NAME ${model_name}.resources
        COMMAND "${Python3_EXECUTABLE}"
            "${PROJECT_SOURCE_DIR}/examples/model/${model_name}/export_model.py"
            --output-dir "${resource_directory}"
            --platform-tag "${FMI_PLATFORM_TAG}"
            --runtime-dir "${FMI_ONNXRUNTIME_ROOT}/lib/${FMI_PLATFORM_TAG}"
    )
    set_tests_properties(${model_name}.resources PROPERTIES
        FIXTURES_SETUP ${model_name}_resources
        TIMEOUT 120
    )

    foreach(test_case IN ITEMS contracts fmi3_contracts parity resource_checks)
        add_test(NAME ${model_name}.${test_case}
            COMMAND ${test_target} ${model_name} "${resource_directory}" ${test_case}
        )
        set_tests_properties(${model_name}.${test_case} PROPERTIES
            FIXTURES_REQUIRED ${model_name}_resources
            TIMEOUT 30
        )
    endforeach()
    # Repeated runtime loads and copies can take longer on busy hosts.
    set_tests_properties(${model_name}.resource_checks PROPERTIES TIMEOUT 120)
endforeach()
