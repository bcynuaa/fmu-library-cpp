add_library(fmi2_core OBJECT
    fmi2_wrapper.cpp
)

fmi_configure_target(fmi2_core)
set_target_properties(fmi2_core PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_compile_definitions(fmi2_core PRIVATE
    FMI2_BUILD
    $<$<CXX_COMPILER_ID:MSVC>:_CRT_SECURE_NO_WARNINGS>
)
