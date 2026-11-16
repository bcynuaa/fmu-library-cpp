#ifndef FMI2_BASE_HPP_
#define FMI2_BASE_HPP_

#include <cstddef>

#include "fmi2/detail/logger.hpp"
#include "fmi2/fmi2_declare.h"

namespace fmi2 {

class Fmi2Base {
    public:
    virtual ~Fmi2Base() = default;

    virtual fmi2Type type() const noexcept = 0;

    void configureLogging(
        fmi2String name, const fmi2CallbackFunctions* callbacks) {
        this->logger_.configure(name, callbacks);
    }
    void logError(const char* operation, const char* reason) const noexcept {
        this->logger_.error(operation, reason);
    }
    virtual const char* lastError() const noexcept { return ""; }
    virtual void clearError() {}

    virtual fmi2Status instantiate(fmi2String instance_name, fmi2Type fmu_type,
        fmi2String fmu_guid, fmi2String fmu_resource_location,
        const fmi2CallbackFunctions* functions, fmi2Boolean visible,
        fmi2Boolean logging_on) {
        (void)instance_name;
        (void)fmu_type;
        (void)fmu_guid;
        (void)fmu_resource_location;
        (void)functions;
        (void)visible;
        (void)logging_on;
        return fmi2OK;
    }

    virtual fmi2Status setDebugLogging(
        fmi2Boolean, std::size_t, const fmi2String[]) {
        return fmi2OK;
    }

    virtual fmi2Status setupExperiment(
        fmi2Boolean, fmi2Real, fmi2Real, fmi2Boolean, fmi2Real) {
        return fmi2OK;
    }

    virtual fmi2Status enterInitializationMode() { return fmi2OK; }
    virtual fmi2Status exitInitializationMode() { return fmi2OK; }
    virtual fmi2Status terminate() { return fmi2OK; }
    virtual fmi2Status reset() { return fmi2OK; }

    virtual fmi2Status getReal(
        const fmi2ValueReference[], std::size_t, fmi2Real[]) {
        return fmi2Error;
    }

    virtual fmi2Status setReal(
        const fmi2ValueReference[], std::size_t, const fmi2Real[]) {
        return fmi2Error;
    }

    virtual fmi2Status getInteger(
        const fmi2ValueReference[], std::size_t, fmi2Integer[]) {
        return fmi2Error;
    }

    virtual fmi2Status setInteger(
        const fmi2ValueReference[], std::size_t, const fmi2Integer[]) {
        return fmi2Error;
    }

    virtual fmi2Status getBoolean(
        const fmi2ValueReference[], std::size_t, fmi2Boolean[]) {
        return fmi2Error;
    }

    virtual fmi2Status setBoolean(
        const fmi2ValueReference[], std::size_t, const fmi2Boolean[]) {
        return fmi2Error;
    }

    virtual fmi2Status getString(
        const fmi2ValueReference[], std::size_t, fmi2String[]) {
        return fmi2Error;
    }

    virtual fmi2Status setString(
        const fmi2ValueReference[], std::size_t, const fmi2String[]) {
        return fmi2Error;
    }

    virtual fmi2Status getFmuState(fmi2FMUstate*) { return fmi2Error; }
    virtual fmi2Status setFmuState(fmi2FMUstate) { return fmi2Error; }
    virtual fmi2Status freeFmuState(fmi2FMUstate*) { return fmi2Error; }

    virtual fmi2Status serializedFmuStateSize(fmi2FMUstate, std::size_t*) {
        return fmi2Error;
    }

    virtual fmi2Status serializeFmuState(
        fmi2FMUstate, fmi2Byte[], std::size_t) {
        return fmi2Error;
    }

    virtual fmi2Status deserializeFmuState(
        const fmi2Byte[], std::size_t, fmi2FMUstate*) {
        return fmi2Error;
    }

    virtual fmi2Status getDirectionalDerivative(const fmi2ValueReference[],
        std::size_t, const fmi2ValueReference[], std::size_t, const fmi2Real[],
        fmi2Real[]) {
        return fmi2Error;
    }

    private:
    detail::Logger logger_;
};

}  // namespace fmi2

#endif  // FMI2_BASE_HPP_
