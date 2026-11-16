#include <cmath>
#include <cstddef>
#include <exception>
#include <memory>

#include "fmi3/cs_base.hpp"
#include "fmi3/factory.hpp"
#include "fmi3/fmi3_declare.h"

namespace {

fmi3::Fmi3Base* base(fmi3Instance instance) {
    return static_cast<fmi3::Fmi3Base*>(instance);
}

fmi3Status reject(fmi3Instance instance, const char* operation) {
    if (base(instance))
        base(instance)->logError(
            operation, "Invalid references, buffer, or arguments");
    return fmi3Error;
}

fmi3Status unsupported(fmi3Instance instance, const char* operation) {
    if (base(instance))
        base(instance)->logError(operation,
            "Model Exchange and Scheduled Execution are unsupported");
    return fmi3Error;
}

template <typename T>
bool validBuffer(const T* values, std::size_t count) {
    return count == 0 || values != nullptr;
}

bool validReferences(const fmi3ValueReference references[], std::size_t count) {
    if (!validBuffer(references, count)) return false;
    for (std::size_t index = 0; index < count; ++index) {
        if (references[index] == 0) return false;
    }
    return true;
}

template <typename Action>
fmi3Status callModel(
    fmi3Instance instance, const char* operation, Action&& action) {
    auto* model = base(instance);
    if (!model) return fmi3Error;
    try {
        model->clearError();
        const auto status = action(*model);
        if (status == fmi3Error || status == fmi3Fatal)
            model->logError(operation, model->lastError());
        return status;
    } catch (const std::exception& error) {
        model->logError(operation, error.what());
        return fmi3Error;
    } catch (...) {
        model->logError(operation, "Unknown C++ exception");
        return fmi3Error;
    }
}

template <typename Action>
fmi3Status callCoSimulation(
    fmi3Instance instance, const char* operation, Action&& action) {
    try {
        auto* model = dynamic_cast<fmi3::CsBase*>(base(instance));
        if (!model) return unsupported(instance, operation);
        model->clearError();
        const auto status = action(*model);
        if (status == fmi3Error || status == fmi3Fatal)
            model->logError(operation, model->lastError());
        return status;
    } catch (const std::exception& error) {
        if (base(instance)) base(instance)->logError(operation, error.what());
        return fmi3Error;
    } catch (...) {
        if (base(instance))
            base(instance)->logError(operation, "Unknown C++ exception");
        return fmi3Error;
    }
}

}  // namespace

extern "C" {

const char* FMI3_CALL fmi3GetVersion(void) { return fmi3Version; }

fmi3Instance FMI3_CALL fmi3InstantiateCoSimulation(fmi3String instance_name,
    fmi3String instantiation_token, fmi3String resource_path,
    fmi3Boolean visible, fmi3Boolean logging_on, fmi3Boolean event_mode_used,
    fmi3Boolean early_return_allowed,
    const fmi3ValueReference required_intermediate_variables[],
    size_t n_required_intermediate_variables,
    fmi3InstanceEnvironment instance_environment,
    fmi3LogMessageCallback log_message,
    fmi3IntermediateUpdateCallback intermediate_update) {
    fmi3::detail::Logger logger;
    logger.configure(instance_environment, log_message);
    // * FMI 3 allows a NULL resource path for FMUs without resources; models
    // * that need one report the reason from their own load().
    if (!instance_name || !instance_name[0] || !instantiation_token ||
        !instantiation_token[0] ||
        !validReferences(required_intermediate_variables,
            n_required_intermediate_variables)) {
        logger.error(__func__,
            "Invalid instance name, token, or intermediate references");
        return nullptr;
    }
    try {
        std::unique_ptr<fmi3::Fmi3Base> model(
            fmi3::createFmi3Instance(fmi3::InterfaceType::kCoSimulation));
        if (!model || model->type() != fmi3::InterfaceType::kCoSimulation) {
            logger.error(__func__, "Unsupported FMI interface");
            return nullptr;
        }
        auto* cs_model = dynamic_cast<fmi3::CsBase*>(model.get());
        if (!cs_model) {
            logger.error(__func__, "Unsupported FMI interface");
            return nullptr;
        }
        model->configureLogging(instance_environment, log_message);
        const fmi3Status status = cs_model->instantiate(instance_name,
            instantiation_token, resource_path, visible, logging_on,
            event_mode_used, early_return_allowed,
            required_intermediate_variables, n_required_intermediate_variables,
            instance_environment, log_message, intermediate_update);
        if (status != fmi3OK && status != fmi3Warning) {
            model->logError(__func__, model->lastError());
            return nullptr;
        }
        return static_cast<fmi3Instance>(model.release());
    } catch (const std::exception& error) {
        logger.error(__func__, error.what());
        return nullptr;
    } catch (...) {
        logger.error(__func__, "Unknown C++ exception");
        return nullptr;
    }
}

void FMI3_CALL fmi3FreeInstance(fmi3Instance instance) {
    try {
        delete base(instance);
    } catch (...) {
    }
}

// Counts are forwarded independently so one reference may address an array.
#define FMI3_DEFINE_VALUE_ACCESSORS(TypeName, ValueType, GetMethod, SetMethod) \
    fmi3Status FMI3_CALL fmi3Get##TypeName(fmi3Instance instance,              \
        const fmi3ValueReference value_references[],                           \
        size_t n_value_references, ValueType values[], size_t n_values) {      \
        if (!validReferences(value_references, n_value_references) ||          \
            !validBuffer(values, n_values))                                    \
            return reject(instance, __func__);                                 \
        return callModel(instance, __func__, [&](fmi3::Fmi3Base& model) {      \
            return model.GetMethod(                                            \
                value_references, n_value_references, values, n_values);       \
        });                                                                    \
    }                                                                          \
    fmi3Status FMI3_CALL fmi3Set##TypeName(fmi3Instance instance,              \
        const fmi3ValueReference value_references[],                           \
        size_t n_value_references, const ValueType values[],                   \
        size_t n_values) {                                                     \
        if (!validReferences(value_references, n_value_references) ||          \
            !validBuffer(values, n_values))                                    \
            return reject(instance, __func__);                                 \
        return callModel(instance, __func__, [&](fmi3::Fmi3Base& model) {      \
            return model.SetMethod(                                            \
                value_references, n_value_references, values, n_values);       \
        });                                                                    \
    }

FMI3_DEFINE_VALUE_ACCESSORS(Float32, fmi3Float32, getFloat32, setFloat32)
FMI3_DEFINE_VALUE_ACCESSORS(Float64, fmi3Float64, getFloat64, setFloat64)
FMI3_DEFINE_VALUE_ACCESSORS(Int8, fmi3Int8, getInt8, setInt8)
FMI3_DEFINE_VALUE_ACCESSORS(UInt8, fmi3UInt8, getUInt8, setUInt8)
FMI3_DEFINE_VALUE_ACCESSORS(Int16, fmi3Int16, getInt16, setInt16)
FMI3_DEFINE_VALUE_ACCESSORS(UInt16, fmi3UInt16, getUInt16, setUInt16)
FMI3_DEFINE_VALUE_ACCESSORS(Int32, fmi3Int32, getInt32, setInt32)
FMI3_DEFINE_VALUE_ACCESSORS(UInt32, fmi3UInt32, getUInt32, setUInt32)
FMI3_DEFINE_VALUE_ACCESSORS(Int64, fmi3Int64, getInt64, setInt64)
FMI3_DEFINE_VALUE_ACCESSORS(UInt64, fmi3UInt64, getUInt64, setUInt64)
FMI3_DEFINE_VALUE_ACCESSORS(Boolean, fmi3Boolean, getBoolean, setBoolean)
FMI3_DEFINE_VALUE_ACCESSORS(String, fmi3String, getString, setString)

#undef FMI3_DEFINE_VALUE_ACCESSORS

fmi3Status FMI3_CALL fmi3SetDebugLogging(fmi3Instance instance,
    fmi3Boolean logging_on, size_t n_categories,
    const fmi3String categories[]) {
    if (!validBuffer(categories, n_categories))
        return reject(instance, __func__);
    return callModel(instance, __func__, [&](fmi3::Fmi3Base& model) {
        return model.setDebugLogging(logging_on, n_categories, categories);
    });
}

fmi3Status FMI3_CALL fmi3EnterInitializationMode(fmi3Instance instance,
    fmi3Boolean tolerance_defined, fmi3Float64 tolerance,
    fmi3Float64 start_time, fmi3Boolean stop_time_defined,
    fmi3Float64 stop_time) {
    return callModel(instance, __func__, [&](fmi3::Fmi3Base& model) {
        return model.enterInitializationMode(tolerance_defined, tolerance,
            start_time, stop_time_defined, stop_time);
    });
}

fmi3Status FMI3_CALL fmi3ExitInitializationMode(fmi3Instance instance) {
    return callModel(instance, __func__,
        [&](fmi3::Fmi3Base& model) { return model.exitInitializationMode(); });
}

fmi3Status FMI3_CALL fmi3EnterEventMode(fmi3Instance instance) {
    return callModel(instance, __func__,
        [&](fmi3::Fmi3Base& model) { return model.enterEventMode(); });
}

fmi3Status FMI3_CALL fmi3Terminate(fmi3Instance instance) {
    return callModel(instance, __func__,
        [&](fmi3::Fmi3Base& model) { return model.terminate(); });
}

fmi3Status FMI3_CALL fmi3Reset(fmi3Instance instance) {
    return callModel(instance, __func__,
        [&](fmi3::Fmi3Base& model) { return model.reset(); });
}

fmi3Status FMI3_CALL fmi3GetBinary(fmi3Instance instance,
    const fmi3ValueReference value_references[], size_t n_value_references,
    size_t value_sizes[], fmi3Binary values[], size_t n_values) {
    if (!validReferences(value_references, n_value_references) ||
        !validBuffer(value_sizes, n_values) || !validBuffer(values, n_values))
        return reject(instance, __func__);
    return callModel(instance, __func__, [&](fmi3::Fmi3Base& model) {
        return model.getBinary(value_references, n_value_references,
            value_sizes, values, n_values);
    });
}

fmi3Status FMI3_CALL fmi3GetClock(fmi3Instance instance,
    const fmi3ValueReference value_references[], size_t n_value_references,
    fmi3Clock values[]) {
    if (!validReferences(value_references, n_value_references) ||
        !validBuffer(values, n_value_references))
        return reject(instance, __func__);
    return callModel(instance, __func__, [&](fmi3::Fmi3Base& model) {
        return model.getClock(value_references, n_value_references, values);
    });
}

fmi3Status FMI3_CALL fmi3SetBinary(fmi3Instance instance,
    const fmi3ValueReference value_references[], size_t n_value_references,
    const size_t value_sizes[], const fmi3Binary values[], size_t n_values) {
    if (!validReferences(value_references, n_value_references) ||
        !validBuffer(value_sizes, n_values) || !validBuffer(values, n_values))
        return reject(instance, __func__);
    for (size_t index = 0; index < n_values; ++index) {
        if (!validBuffer(values[index], value_sizes[index]))
            return reject(instance, __func__);
    }
    return callModel(instance, __func__, [&](fmi3::Fmi3Base& model) {
        return model.setBinary(value_references, n_value_references,
            value_sizes, values, n_values);
    });
}

fmi3Status FMI3_CALL fmi3SetClock(fmi3Instance instance,
    const fmi3ValueReference value_references[], size_t n_value_references,
    const fmi3Clock values[]) {
    if (!validReferences(value_references, n_value_references) ||
        !validBuffer(values, n_value_references))
        return reject(instance, __func__);
    return callModel(instance, __func__, [&](fmi3::Fmi3Base& model) {
        return model.setClock(value_references, n_value_references, values);
    });
}

fmi3Status FMI3_CALL fmi3GetNumberOfVariableDependencies(fmi3Instance instance,
    fmi3ValueReference value_reference, size_t* n_dependencies) {
    if (value_reference == 0 || !n_dependencies)
        return reject(instance, __func__);
    return callModel(instance, __func__, [&](fmi3::Fmi3Base& model) {
        return model.getNumberOfVariableDependencies(
            value_reference, n_dependencies);
    });
}

fmi3Status FMI3_CALL fmi3GetVariableDependencies(fmi3Instance instance,
    fmi3ValueReference dependent, size_t element_indices_of_dependent[],
    fmi3ValueReference independents[], size_t element_indices_of_independents[],
    fmi3DependencyKind dependency_kinds[], size_t n_dependencies) {
    if (dependent == 0 ||
        !validBuffer(element_indices_of_dependent, n_dependencies) ||
        !validBuffer(independents, n_dependencies) ||
        !validBuffer(element_indices_of_independents, n_dependencies) ||
        !validBuffer(dependency_kinds, n_dependencies))
        return reject(instance, __func__);
    return callModel(instance, __func__, [&](fmi3::Fmi3Base& model) {
        return model.getVariableDependencies(dependent,
            element_indices_of_dependent, independents,
            element_indices_of_independents, dependency_kinds, n_dependencies);
    });
}

fmi3Status FMI3_CALL fmi3GetFMUState(
    fmi3Instance instance, fmi3FMUState* fmu_state) {
    if (!fmu_state) return reject(instance, __func__);
    return callModel(instance, __func__,
        [&](fmi3::Fmi3Base& model) { return model.getFmuState(fmu_state); });
}

fmi3Status FMI3_CALL fmi3SetFMUState(
    fmi3Instance instance, fmi3FMUState fmu_state) {
    if (!fmu_state) return reject(instance, __func__);
    return callModel(instance, __func__,
        [&](fmi3::Fmi3Base& model) { return model.setFmuState(fmu_state); });
}

fmi3Status FMI3_CALL fmi3FreeFMUState(
    fmi3Instance instance, fmi3FMUState* fmu_state) {
    if (!fmu_state) return reject(instance, __func__);
    return callModel(instance, __func__,
        [&](fmi3::Fmi3Base& model) { return model.freeFmuState(fmu_state); });
}

fmi3Status FMI3_CALL fmi3SerializedFMUStateSize(
    fmi3Instance instance, fmi3FMUState fmu_state, size_t* size) {
    if (!fmu_state || !size) return reject(instance, __func__);
    return callModel(instance, __func__, [&](fmi3::Fmi3Base& model) {
        return model.serializedFmuStateSize(fmu_state, size);
    });
}

fmi3Status FMI3_CALL fmi3SerializeFMUState(fmi3Instance instance,
    fmi3FMUState fmu_state, fmi3Byte serialized_state[], size_t size) {
    if (!fmu_state || !validBuffer(serialized_state, size))
        return reject(instance, __func__);
    return callModel(instance, __func__, [&](fmi3::Fmi3Base& model) {
        return model.serializeFmuState(fmu_state, serialized_state, size);
    });
}

fmi3Status FMI3_CALL fmi3DeserializeFMUState(fmi3Instance instance,
    const fmi3Byte serialized_state[], size_t size, fmi3FMUState* fmu_state) {
    if (!validBuffer(serialized_state, size) || !fmu_state)
        return reject(instance, __func__);
    return callModel(instance, __func__, [&](fmi3::Fmi3Base& model) {
        return model.deserializeFmuState(serialized_state, size, fmu_state);
    });
}

fmi3Status FMI3_CALL fmi3GetDirectionalDerivative(fmi3Instance instance,
    const fmi3ValueReference unknowns[], size_t n_unknowns,
    const fmi3ValueReference knowns[], size_t n_knowns,
    const fmi3Float64 seed[], size_t n_seed, fmi3Float64 sensitivity[],
    size_t n_sensitivity) {
    if (!validReferences(unknowns, n_unknowns) ||
        !validReferences(knowns, n_knowns) || !validBuffer(seed, n_seed) ||
        !validBuffer(sensitivity, n_sensitivity))
        return reject(instance, __func__);
    return callModel(instance, __func__, [&](fmi3::Fmi3Base& model) {
        return model.getDirectionalDerivative(unknowns, n_unknowns, knowns,
            n_knowns, seed, n_seed, sensitivity, n_sensitivity);
    });
}

fmi3Status FMI3_CALL fmi3GetAdjointDerivative(fmi3Instance instance,
    const fmi3ValueReference unknowns[], size_t n_unknowns,
    const fmi3ValueReference knowns[], size_t n_knowns,
    const fmi3Float64 seed[], size_t n_seed, fmi3Float64 sensitivity[],
    size_t n_sensitivity) {
    if (!validReferences(unknowns, n_unknowns) ||
        !validReferences(knowns, n_knowns) || !validBuffer(seed, n_seed) ||
        !validBuffer(sensitivity, n_sensitivity))
        return reject(instance, __func__);
    return callModel(instance, __func__, [&](fmi3::Fmi3Base& model) {
        return model.getAdjointDerivative(unknowns, n_unknowns, knowns,
            n_knowns, seed, n_seed, sensitivity, n_sensitivity);
    });
}

fmi3Status FMI3_CALL fmi3EnterConfigurationMode(fmi3Instance instance) {
    return callModel(instance, __func__,
        [&](fmi3::Fmi3Base& model) { return model.enterConfigurationMode(); });
}

fmi3Status FMI3_CALL fmi3ExitConfigurationMode(fmi3Instance instance) {
    return callModel(instance, __func__,
        [&](fmi3::Fmi3Base& model) { return model.exitConfigurationMode(); });
}

fmi3Status FMI3_CALL fmi3GetIntervalDecimal(fmi3Instance instance,
    const fmi3ValueReference value_references[], size_t n_value_references,
    fmi3Float64 intervals[], fmi3IntervalQualifier qualifiers[]) {
    if (!validReferences(value_references, n_value_references) ||
        !validBuffer(intervals, n_value_references) ||
        !validBuffer(qualifiers, n_value_references))
        return reject(instance, __func__);
    return callModel(instance, __func__, [&](fmi3::Fmi3Base& model) {
        return model.getIntervalDecimal(
            value_references, n_value_references, intervals, qualifiers);
    });
}

fmi3Status FMI3_CALL fmi3GetIntervalFraction(fmi3Instance instance,
    const fmi3ValueReference value_references[], size_t n_value_references,
    fmi3UInt64 counters[], fmi3UInt64 resolutions[],
    fmi3IntervalQualifier qualifiers[]) {
    if (!validReferences(value_references, n_value_references) ||
        !validBuffer(counters, n_value_references) ||
        !validBuffer(resolutions, n_value_references) ||
        !validBuffer(qualifiers, n_value_references))
        return reject(instance, __func__);
    return callModel(instance, __func__, [&](fmi3::Fmi3Base& model) {
        return model.getIntervalFraction(value_references, n_value_references,
            counters, resolutions, qualifiers);
    });
}

fmi3Status FMI3_CALL fmi3GetShiftDecimal(fmi3Instance instance,
    const fmi3ValueReference value_references[], size_t n_value_references,
    fmi3Float64 shifts[]) {
    if (!validReferences(value_references, n_value_references) ||
        !validBuffer(shifts, n_value_references))
        return reject(instance, __func__);
    return callModel(instance, __func__, [&](fmi3::Fmi3Base& model) {
        return model.getShiftDecimal(
            value_references, n_value_references, shifts);
    });
}

fmi3Status FMI3_CALL fmi3GetShiftFraction(fmi3Instance instance,
    const fmi3ValueReference value_references[], size_t n_value_references,
    fmi3UInt64 counters[], fmi3UInt64 resolutions[]) {
    if (!validReferences(value_references, n_value_references) ||
        !validBuffer(counters, n_value_references) ||
        !validBuffer(resolutions, n_value_references))
        return reject(instance, __func__);
    return callModel(instance, __func__, [&](fmi3::Fmi3Base& model) {
        return model.getShiftFraction(
            value_references, n_value_references, counters, resolutions);
    });
}

fmi3Status FMI3_CALL fmi3SetIntervalDecimal(fmi3Instance instance,
    const fmi3ValueReference value_references[], size_t n_value_references,
    const fmi3Float64 intervals[]) {
    if (!validReferences(value_references, n_value_references) ||
        !validBuffer(intervals, n_value_references))
        return reject(instance, __func__);
    return callModel(instance, __func__, [&](fmi3::Fmi3Base& model) {
        return model.setIntervalDecimal(
            value_references, n_value_references, intervals);
    });
}

fmi3Status FMI3_CALL fmi3SetIntervalFraction(fmi3Instance instance,
    const fmi3ValueReference value_references[], size_t n_value_references,
    const fmi3UInt64 counters[], const fmi3UInt64 resolutions[]) {
    if (!validReferences(value_references, n_value_references) ||
        !validBuffer(counters, n_value_references) ||
        !validBuffer(resolutions, n_value_references))
        return reject(instance, __func__);
    return callModel(instance, __func__, [&](fmi3::Fmi3Base& model) {
        return model.setIntervalFraction(
            value_references, n_value_references, counters, resolutions);
    });
}

fmi3Status FMI3_CALL fmi3SetShiftDecimal(fmi3Instance instance,
    const fmi3ValueReference value_references[], size_t n_value_references,
    const fmi3Float64 shifts[]) {
    if (!validReferences(value_references, n_value_references) ||
        !validBuffer(shifts, n_value_references))
        return reject(instance, __func__);
    return callModel(instance, __func__, [&](fmi3::Fmi3Base& model) {
        return model.setShiftDecimal(
            value_references, n_value_references, shifts);
    });
}

fmi3Status FMI3_CALL fmi3SetShiftFraction(fmi3Instance instance,
    const fmi3ValueReference value_references[], size_t n_value_references,
    const fmi3UInt64 counters[], const fmi3UInt64 resolutions[]) {
    if (!validReferences(value_references, n_value_references) ||
        !validBuffer(counters, n_value_references) ||
        !validBuffer(resolutions, n_value_references))
        return reject(instance, __func__);
    return callModel(instance, __func__, [&](fmi3::Fmi3Base& model) {
        return model.setShiftFraction(
            value_references, n_value_references, counters, resolutions);
    });
}

fmi3Status FMI3_CALL fmi3EvaluateDiscreteStates(fmi3Instance instance) {
    return callModel(instance, __func__,
        [&](fmi3::Fmi3Base& model) { return model.evaluateDiscreteStates(); });
}

fmi3Status FMI3_CALL fmi3UpdateDiscreteStates(fmi3Instance instance,
    fmi3Boolean* discrete_states_need_update, fmi3Boolean* terminate_simulation,
    fmi3Boolean* nominals_of_continuous_states_changed,
    fmi3Boolean* values_of_continuous_states_changed,
    fmi3Boolean* next_event_time_defined, fmi3Float64* next_event_time) {
    if (!discrete_states_need_update || !terminate_simulation ||
        !nominals_of_continuous_states_changed ||
        !values_of_continuous_states_changed || !next_event_time_defined ||
        !next_event_time)
        return reject(instance, __func__);
    return callModel(instance, __func__, [&](fmi3::Fmi3Base& model) {
        return model.updateDiscreteStates(discrete_states_need_update,
            terminate_simulation, nominals_of_continuous_states_changed,
            values_of_continuous_states_changed, next_event_time_defined,
            next_event_time);
    });
}

fmi3Status FMI3_CALL fmi3EnterStepMode(fmi3Instance instance) {
    return callCoSimulation(instance, __func__,
        [&](fmi3::CsBase& model) { return model.enterStepMode(); });
}

fmi3Status FMI3_CALL fmi3GetOutputDerivatives(fmi3Instance instance,
    const fmi3ValueReference value_references[], size_t n_value_references,
    const fmi3Int32 orders[], fmi3Float64 values[], size_t n_values) {
    if (!validReferences(value_references, n_value_references) ||
        !validBuffer(orders, n_value_references) ||
        !validBuffer(values, n_values))
        return reject(instance, __func__);
    return callCoSimulation(instance, __func__, [&](fmi3::CsBase& model) {
        return model.getOutputDerivatives(
            value_references, n_value_references, orders, values, n_values);
    });
}

fmi3Status FMI3_CALL fmi3DoStep(fmi3Instance instance,
    fmi3Float64 current_communication_point,
    fmi3Float64 communication_step_size,
    fmi3Boolean no_set_fmu_state_prior_to_current_point,
    fmi3Boolean* event_handling_needed, fmi3Boolean* terminate_simulation,
    fmi3Boolean* early_return, fmi3Float64* last_successful_time) {
    if (!event_handling_needed || !terminate_simulation || !early_return ||
        !last_successful_time)
        return reject(instance, __func__);
    *event_handling_needed = fmi3False;
    *terminate_simulation = fmi3False;
    *early_return = fmi3False;
    *last_successful_time = current_communication_point;
    return callCoSimulation(instance, __func__, [&](fmi3::CsBase& model) {
        return model.doStep(current_communication_point,
            communication_step_size, no_set_fmu_state_prior_to_current_point,
            event_handling_needed, terminate_simulation, early_return,
            last_successful_time);
    });
}

// These modes are not implemented by the current CS-only core.

fmi3Instance FMI3_CALL fmi3InstantiateModelExchange(fmi3String, fmi3String,
    fmi3String, fmi3Boolean, fmi3Boolean, fmi3InstanceEnvironment environment,
    fmi3LogMessageCallback callback) {
    fmi3::detail::Logger logger;
    logger.configure(environment, callback);
    logger.error(__func__, "Model Exchange is unsupported");
    return nullptr;
}

fmi3Instance FMI3_CALL fmi3InstantiateScheduledExecution(fmi3String, fmi3String,
    fmi3String, fmi3Boolean, fmi3Boolean, fmi3InstanceEnvironment environment,
    fmi3LogMessageCallback callback, fmi3ClockUpdateCallback,
    fmi3LockPreemptionCallback, fmi3UnlockPreemptionCallback) {
    fmi3::detail::Logger logger;
    logger.configure(environment, callback);
    logger.error(__func__, "Scheduled Execution is unsupported");
    return nullptr;
}

fmi3Status FMI3_CALL fmi3EnterContinuousTimeMode(fmi3Instance instance) {
    return unsupported(instance, __func__);
}

fmi3Status FMI3_CALL fmi3CompletedIntegratorStep(
    fmi3Instance instance, fmi3Boolean, fmi3Boolean*, fmi3Boolean*) {
    return unsupported(instance, __func__);
}

fmi3Status FMI3_CALL fmi3SetTime(fmi3Instance instance, fmi3Float64) {
    return unsupported(instance, __func__);
}

fmi3Status FMI3_CALL fmi3SetContinuousStates(
    fmi3Instance instance, const fmi3Float64[], size_t) {
    return unsupported(instance, __func__);
}

fmi3Status FMI3_CALL fmi3GetContinuousStateDerivatives(
    fmi3Instance instance, fmi3Float64[], size_t) {
    return unsupported(instance, __func__);
}

fmi3Status FMI3_CALL fmi3GetEventIndicators(
    fmi3Instance instance, fmi3Float64[], size_t) {
    return unsupported(instance, __func__);
}

fmi3Status FMI3_CALL fmi3GetContinuousStates(
    fmi3Instance instance, fmi3Float64[], size_t) {
    return unsupported(instance, __func__);
}

fmi3Status FMI3_CALL fmi3GetNominalsOfContinuousStates(
    fmi3Instance instance, fmi3Float64[], size_t) {
    return unsupported(instance, __func__);
}

fmi3Status FMI3_CALL fmi3GetNumberOfEventIndicators(
    fmi3Instance instance, size_t*) {
    return unsupported(instance, __func__);
}

fmi3Status FMI3_CALL fmi3GetNumberOfContinuousStates(
    fmi3Instance instance, size_t*) {
    return unsupported(instance, __func__);
}

fmi3Status FMI3_CALL fmi3ActivateModelPartition(
    fmi3Instance instance, fmi3ValueReference, fmi3Float64) {
    return unsupported(instance, __func__);
}

}  // extern "C"
