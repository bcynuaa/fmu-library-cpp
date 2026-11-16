add_library(fmi3_core OBJECT
    fmi3_wrapper.cpp
)

fmi_configure_target(fmi3_core)
set_target_properties(fmi3_core PROPERTIES POSITION_INDEPENDENT_CODE ON)

target_compile_definitions(fmi3_core PRIVATE
    FMI3_BUILD
    $<$<CXX_COMPILER_ID:MSVC>:_CRT_SECURE_NO_WARNINGS>
)
