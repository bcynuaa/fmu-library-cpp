#ifndef FMI3_BASE_HPP_
#define FMI3_BASE_HPP_

#include <cstddef>

#include "fmi3/detail/logger.hpp"
#include "fmi3/fmi3_declare.h"

namespace fmi3 {

// Internal discriminator; FMI 3 uses separate C entry points for instantiation.
enum class InterfaceType {
    kModelExchange,
    kCoSimulation,
    kScheduledExecution,
};

// Common dispatch interface. Concrete models own lifecycle state and advertise
// only the capabilities they implement in modelDescription.xml.
class Fmi3Base {
    public:
    virtual ~Fmi3Base() = default;

    virtual InterfaceType type() const noexcept = 0;

    void configureLogging(fmi3InstanceEnvironment environment,
        fmi3LogMessageCallback callback) noexcept {
        this->logger_.configure(environment, callback);
    }
    void logError(const char* operation, const char* reason) const noexcept {
        this->logger_.error(operation, reason);
    }
    virtual const char* lastError() const noexcept { return ""; }
    virtual void clearError() {}

    virtual fmi3Status setDebugLogging(
        fmi3Boolean, std::size_t, const fmi3String[]) {
        return fmi3OK;
    }

    virtual fmi3Status enterInitializationMode(
        fmi3Boolean, fmi3Float64, fmi3Float64, fmi3Boolean, fmi3Float64) {
        return fmi3OK;
    }

    virtual fmi3Status exitInitializationMode() { return fmi3OK; }

    virtual fmi3Status enterEventMode() { return fmi3Error; }

    virtual fmi3Status terminate() { return fmi3OK; }

    virtual fmi3Status reset() { return fmi3OK; }

    virtual fmi3Status getFloat32(
        const fmi3ValueReference[], std::size_t, fmi3Float32[], std::size_t) {
        return fmi3Error;
    }

    virtual fmi3Status getFloat64(
        const fmi3ValueReference[], std::size_t, fmi3Float64[], std::size_t) {
        return fmi3Error;
    }

    virtual fmi3Status getInt8(
        const fmi3ValueReference[], std::size_t, fmi3Int8[], std::size_t) {
        return fmi3Error;
    }

    virtual fmi3Status getUInt8(
        const fmi3ValueReference[], std::size_t, fmi3UInt8[], std::size_t) {
        return fmi3Error;
    }

    virtual fmi3Status getInt16(
        const fmi3ValueReference[], std::size_t, fmi3Int16[], std::size_t) {
        return fmi3Error;
    }

    virtual fmi3Status getUInt16(
        const fmi3ValueReference[], std::size_t, fmi3UInt16[], std::size_t) {
        return fmi3Error;
    }

    virtual fmi3Status getInt32(
        const fmi3ValueReference[], std::size_t, fmi3Int32[], std::size_t) {
        return fmi3Error;
    }

    virtual fmi3Status getUInt32(
        const fmi3ValueReference[], std::size_t, fmi3UInt32[], std::size_t) {
        return fmi3Error;
    }

    virtual fmi3Status getInt64(
        const fmi3ValueReference[], std::size_t, fmi3Int64[], std::size_t) {
        return fmi3Error;
    }

    virtual fmi3Status getUInt64(
        const fmi3ValueReference[], std::size_t, fmi3UInt64[], std::size_t) {
        return fmi3Error;
    }

    virtual fmi3Status getBoolean(
        const fmi3ValueReference[], std::size_t, fmi3Boolean[], std::size_t) {
        return fmi3Error;
    }

    virtual fmi3Status getString(
        const fmi3ValueReference[], std::size_t, fmi3String[], std::size_t) {
        return fmi3Error;
    }

    virtual fmi3Status getBinary(const fmi3ValueReference[], std::size_t,
        std::size_t[], fmi3Binary[], std::size_t) {
        return fmi3Error;
    }

    virtual fmi3Status getClock(
        const fmi3ValueReference[], std::size_t, fmi3Clock[]) {
        return fmi3Error;
    }

    virtual fmi3Status setFloat32(const fmi3ValueReference[], std::size_t,
        const fmi3Float32[], std::size_t) {
        return fmi3Error;
    }

    virtual fmi3Status setFloat64(const fmi3ValueReference[], std::size_t,
        const fmi3Float64[], std::size_t) {
        return fmi3Error;
    }

    virtual fmi3Status setInt8(const fmi3ValueReference[], std::size_t,
        const fmi3Int8[], std::size_t) {
        return fmi3Error;
    }

    virtual fmi3Status setUInt8(const fmi3ValueReference[], std::size_t,
        const fmi3UInt8[], std::size_t) {
        return fmi3Error;
    }

    virtual fmi3Status setInt16(const fmi3ValueReference[], std::size_t,
        const fmi3Int16[], std::size_t) {
        return fmi3Error;
    }

    virtual fmi3Status setUInt16(const fmi3ValueReference[], std::size_t,
        const fmi3UInt16[], std::size_t) {
        return fmi3Error;
    }

    virtual fmi3Status setInt32(const fmi3ValueReference[], std::size_t,
        const fmi3Int32[], std::size_t) {
        return fmi3Error;
    }

    virtual fmi3Status setUInt32(const fmi3ValueReference[], std::size_t,
        const fmi3UInt32[], std::size_t) {
        return fmi3Error;
    }

    virtual fmi3Status setInt64(const fmi3ValueReference[], std::size_t,
        const fmi3Int64[], std::size_t) {
        return fmi3Error;
    }

    virtual fmi3Status setUInt64(const fmi3ValueReference[], std::size_t,
        const fmi3UInt64[], std::size_t) {
        return fmi3Error;
    }

    virtual fmi3Status setBoolean(const fmi3ValueReference[], std::size_t,
        const fmi3Boolean[], std::size_t) {
        return fmi3Error;
    }

    virtual fmi3Status setString(const fmi3ValueReference[], std::size_t,
        const fmi3String[], std::size_t) {
        return fmi3Error;
    }

    virtual fmi3Status setBinary(const fmi3ValueReference[], std::size_t,
        const std::size_t[], const fmi3Binary[], std::size_t) {
        return fmi3Error;
    }

    virtual fmi3Status setClock(
        const fmi3ValueReference[], std::size_t, const fmi3Clock[]) {
        return fmi3Error;
    }

    virtual fmi3Status getNumberOfVariableDependencies(
        fmi3ValueReference, std::size_t*) {
        return fmi3Error;
    }

    virtual fmi3Status getVariableDependencies(fmi3ValueReference,
        std::size_t[], fmi3ValueReference[], std::size_t[],
        fmi3DependencyKind[], std::size_t) {
        return fmi3Error;
    }

    virtual fmi3Status getFmuState(fmi3FMUState*) { return fmi3Error; }

    virtual fmi3Status setFmuState(fmi3FMUState) { return fmi3Error; }

    virtual fmi3Status freeFmuState(fmi3FMUState*) { return fmi3Error; }

    virtual fmi3Status serializedFmuStateSize(fmi3FMUState, std::size_t*) {
        return fmi3Error;
    }

    virtual fmi3Status serializeFmuState(
        fmi3FMUState, fmi3Byte[], std::size_t) {
        return fmi3Error;
    }

    virtual fmi3Status deserializeFmuState(
        const fmi3Byte[], std::size_t, fmi3FMUState*) {
        return fmi3Error;
    }

    virtual fmi3Status getDirectionalDerivative(const fmi3ValueReference[],
        std::size_t, const fmi3ValueReference[], std::size_t,
        const fmi3Float64[], std::size_t, fmi3Float64[], std::size_t) {
        return fmi3Error;
    }

    virtual fmi3Status getAdjointDerivative(const fmi3ValueReference[],
        std::size_t, const fmi3ValueReference[], std::size_t,
        const fmi3Float64[], std::size_t, fmi3Float64[], std::size_t) {
        return fmi3Error;
    }

    virtual fmi3Status enterConfigurationMode() { return fmi3Error; }

    virtual fmi3Status exitConfigurationMode() { return fmi3Error; }

    virtual fmi3Status getIntervalDecimal(const fmi3ValueReference[],
        std::size_t, fmi3Float64[], fmi3IntervalQualifier[]) {
        return fmi3Error;
    }

    virtual fmi3Status getIntervalFraction(const fmi3ValueReference[],
        std::size_t, fmi3UInt64[], fmi3UInt64[], fmi3IntervalQualifier[]) {
        return fmi3Error;
    }

    virtual fmi3Status getShiftDecimal(
        const fmi3ValueReference[], std::size_t, fmi3Float64[]) {
        return fmi3Error;
    }

    virtual fmi3Status getShiftFraction(
        const fmi3ValueReference[], std::size_t, fmi3UInt64[], fmi3UInt64[]) {
        return fmi3Error;
    }

    virtual fmi3Status setIntervalDecimal(
        const fmi3ValueReference[], std::size_t, const fmi3Float64[]) {
        return fmi3Error;
    }

    virtual fmi3Status setIntervalFraction(const fmi3ValueReference[],
        std::size_t, const fmi3UInt64[], const fmi3UInt64[]) {
        return fmi3Error;
    }

    virtual fmi3Status setShiftDecimal(
        const fmi3ValueReference[], std::size_t, const fmi3Float64[]) {
        return fmi3Error;
    }

    virtual fmi3Status setShiftFraction(const fmi3ValueReference[], std::size_t,
        const fmi3UInt64[], const fmi3UInt64[]) {
        return fmi3Error;
    }

    virtual fmi3Status evaluateDiscreteStates() { return fmi3Error; }

    virtual fmi3Status updateDiscreteStates(fmi3Boolean*, fmi3Boolean*,
        fmi3Boolean*, fmi3Boolean*, fmi3Boolean*, fmi3Float64*) {
        return fmi3Error;
    }

    private:
    detail::Logger logger_;
};

}  // namespace fmi3

#endif  // FMI3_BASE_HPP_
