#ifndef FMI2_CS_BASE_HPP_
#define FMI2_CS_BASE_HPP_

#include <cstddef>

#include "fmi2/fmi2_base.hpp"

namespace fmi2 {

class CsBase : public Fmi2Base {
    public:
    fmi2Type type() const noexcept final { return fmi2CoSimulation; }

    virtual fmi2Status doStep(fmi2Real, fmi2Real, fmi2Boolean) {
        return fmi2Error;
    }

    virtual fmi2Status cancelStep() { return fmi2Error; }
    virtual fmi2Status getStatus(const fmi2StatusKind, fmi2Status*) {
        return fmi2Error;
    }
    virtual fmi2Status getRealStatus(const fmi2StatusKind, fmi2Real*) {
        return fmi2Error;
    }
    virtual fmi2Status getIntegerStatus(const fmi2StatusKind, fmi2Integer*) {
        return fmi2Error;
    }
    virtual fmi2Status getBooleanStatus(const fmi2StatusKind, fmi2Boolean*) {
        return fmi2Error;
    }
    virtual fmi2Status getStringStatus(const fmi2StatusKind, fmi2String*) {
        return fmi2Error;
    }

    virtual fmi2Status setRealInputDerivatives(const fmi2ValueReference[],
        std::size_t, const fmi2Integer[], const fmi2Real[]) {
        return fmi2Error;
    }

    virtual fmi2Status getRealOutputDerivatives(const fmi2ValueReference[],
        std::size_t, const fmi2Integer[], fmi2Real[]) {
        return fmi2Error;
    }
};

}  // namespace fmi2

#endif  // FMI2_CS_BASE_HPP_
