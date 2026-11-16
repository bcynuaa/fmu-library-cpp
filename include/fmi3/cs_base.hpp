#ifndef FMI3_CS_BASE_HPP_
#define FMI3_CS_BASE_HPP_

#include <cstddef>

#include "fmi3/fmi3_base.hpp"

namespace fmi3 {

class CsBase : public Fmi3Base {
    public:
    InterfaceType type() const noexcept final {
        return InterfaceType::kCoSimulation;
    }

    // resource_path is a UTF-8 filesystem path, not an FMI 2 resource URI.
    // The concrete model must handle or reject requested optional features.
    virtual fmi3Status instantiate(fmi3String instance_name,
        fmi3String instantiation_token, fmi3String resource_path,
        fmi3Boolean visible, fmi3Boolean logging_on,
        fmi3Boolean event_mode_used, fmi3Boolean early_return_allowed,
        const fmi3ValueReference required_intermediate_variables[],
        std::size_t n_required_intermediate_variables,
        fmi3InstanceEnvironment instance_environment,
        fmi3LogMessageCallback log_message,
        fmi3IntermediateUpdateCallback intermediate_update) = 0;

    virtual fmi3Status enterStepMode() { return fmi3Error; }

    virtual fmi3Status getOutputDerivatives(const fmi3ValueReference[],
        std::size_t, const fmi3Int32[], fmi3Float64[], std::size_t) {
        return fmi3Error;
    }

    // Implementations fill all four result outputs on a successful step.
    virtual fmi3Status doStep(fmi3Float64, fmi3Float64, fmi3Boolean,
        fmi3Boolean*, fmi3Boolean*, fmi3Boolean*, fmi3Float64*) {
        return fmi3Error;
    }
};

}  // namespace fmi3

#endif  // FMI3_CS_BASE_HPP_
