#ifndef FMI2_ME_BASE_HPP_
#define FMI2_ME_BASE_HPP_

#include <cstddef>

#include "fmi2/fmi2_base.hpp"

namespace fmi2 {

class MeBase : public Fmi2Base {
    public:
    fmi2Type type() const noexcept final { return fmi2ModelExchange; }

    virtual fmi2Status setTime(fmi2Real) { return fmi2Error; }
    virtual fmi2Status setContinuousStates(const fmi2Real[], std::size_t) {
        return fmi2Error;
    }
    virtual fmi2Status enterEventMode() { return fmi2Error; }
    virtual fmi2Status newDiscreteStates(fmi2EventInfo*) { return fmi2Error; }
    virtual fmi2Status enterContinuousTimeMode() { return fmi2Error; }
    virtual fmi2Status completedIntegratorStep(
        fmi2Boolean, fmi2Boolean*, fmi2Boolean*) {
        return fmi2Error;
    }
    virtual fmi2Status getDerivatives(fmi2Real[], std::size_t) {
        return fmi2Error;
    }
    virtual fmi2Status getContinuousStates(fmi2Real[], std::size_t) {
        return fmi2Error;
    }
    virtual fmi2Status getEventIndicators(fmi2Real[], std::size_t) {
        return fmi2Error;
    }
    virtual fmi2Status getNominalsOfContinuousStates(fmi2Real[], std::size_t) {
        return fmi2Error;
    }
};

}  // namespace fmi2

#endif  // FMI2_ME_BASE_HPP_
