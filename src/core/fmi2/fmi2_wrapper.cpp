#include <exception>
#include <memory>

#include "fmi2/cs_base.hpp"
#include "fmi2/factory.hpp"
#include "fmi2/fmi2_declare.h"
#include "fmi2/me_base.hpp"

namespace {

fmi2::Fmi2Base* base(fmi2Component component) {
    return static_cast<fmi2::Fmi2Base*>(component);
}

template <typename T>
T* modelAs(fmi2Component component) {
    return dynamic_cast<T*>(base(component));
}

template <typename Action>
fmi2Status callModel(
    fmi2Component component, const char* operation, Action&& action) {
    auto* model = base(component);
    if (!model) return fmi2Error;

    try {
        model->clearError();
        const auto status = action(*model);
        if (status == fmi2Error || status == fmi2Fatal)
            model->logError(operation, model->lastError());
        return status;
    } catch (const std::exception& error) {
        model->logError(operation, error.what());
        return fmi2Error;
    } catch (...) {
        model->logError(operation, "Unknown C++ exception");
        return fmi2Error;
    }
}

template <typename Model, typename Action>
fmi2Status callMode(
    fmi2Component component, const char* operation, Action&& action) {
    auto* model = modelAs<Model>(component);
    if (!model) {
        if (base(component))
            base(component)->logError(operation, "Unsupported FMI interface");
        return fmi2Error;
    }

    try {
        model->clearError();
        const auto status = action(*model);
        if (status == fmi2Error || status == fmi2Fatal)
            model->logError(operation, model->lastError());
        return status;
    } catch (const std::exception& error) {
        model->logError(operation, error.what());
        return fmi2Error;
    } catch (...) {
        model->logError(operation, "Unknown C++ exception");
        return fmi2Error;
    }
}

}  // namespace

extern "C" {

fmi2String FMI2_CALL fmi2GetTypesPlatform(void) { return "default"; }

fmi2String FMI2_CALL fmi2GetVersion(void) { return "2.0"; }

fmi2Status FMI2_CALL fmi2SetDebugLogging(fmi2Component c,
    fmi2Boolean logging_on, size_t nCategories, const fmi2String categories[]) {
    return callModel(c, __func__, [&](fmi2::Fmi2Base& model) {
        return model.setDebugLogging(logging_on, nCategories, categories);
    });
}

fmi2Component FMI2_CALL fmi2Instantiate(fmi2String instance_name,
    fmi2Type fmu_type, fmi2String fmu_guid, fmi2String fmu_resource_location,
    const fmi2CallbackFunctions* functions, fmi2Boolean visible,
    fmi2Boolean logging_on) {
    fmi2::detail::Logger logger;
    std::unique_ptr<fmi2::Fmi2Base> model;
    try {
        logger.configure(instance_name, functions);
        // * FMI 2 allows a NULL resource location for FMUs without resources;
        // * models that need one report the reason from their own load().
        if (!instance_name || !instance_name[0] || !fmu_guid || !fmu_guid[0]) {
            logger.error(__func__, "Instance name and GUID are required");
            return nullptr;
        }
        model.reset(fmi2::createFmi2Instance(fmu_type));
        if (!model || model->type() != fmu_type) {
            logger.error(__func__, "Unsupported FMI interface");
            return nullptr;
        }
        model->configureLogging(instance_name, functions);

        const fmi2Status status = model->instantiate(instance_name, fmu_type,
            fmu_guid, fmu_resource_location, functions, visible, logging_on);
        if (status != fmi2OK) {
            model->logError(__func__, model->lastError());
            return nullptr;
        }

        return static_cast<fmi2Component>(model.release());
    } catch (const std::exception& error) {
        logger.error(__func__, error.what());
        return nullptr;
    } catch (...) {
        logger.error(__func__, "Unknown C++ exception");
        return nullptr;
    }
}

void FMI2_CALL fmi2FreeInstance(fmi2Component c) {
    try {
        delete base(c);
    } catch (...) {
    }
}

fmi2Status FMI2_CALL fmi2SetupExperiment(fmi2Component c,
    fmi2Boolean tolerance_defined, fmi2Real tolerance, fmi2Real start_time,
    fmi2Boolean stop_time_defined, fmi2Real stop_time) {
    return callModel(c, __func__, [&](fmi2::Fmi2Base& model) {
        return model.setupExperiment(tolerance_defined, tolerance, start_time,
            stop_time_defined, stop_time);
    });
}

fmi2Status FMI2_CALL fmi2EnterInitializationMode(fmi2Component c) {
    return callModel(c, __func__,
        [](fmi2::Fmi2Base& model) { return model.enterInitializationMode(); });
}

fmi2Status FMI2_CALL fmi2ExitInitializationMode(fmi2Component c) {
    return callModel(c, __func__,
        [](fmi2::Fmi2Base& model) { return model.exitInitializationMode(); });
}

fmi2Status FMI2_CALL fmi2Terminate(fmi2Component c) {
    return callModel(
        c, __func__, [](fmi2::Fmi2Base& model) { return model.terminate(); });
}

fmi2Status FMI2_CALL fmi2Reset(fmi2Component c) {
    return callModel(
        c, __func__, [](fmi2::Fmi2Base& model) { return model.reset(); });
}

#define FMI2_DEFINE_VALUE_ACCESSORS(TypeName, ValueType, GetMethod, SetMethod) \
    fmi2Status FMI2_CALL fmi2Get##TypeName(fmi2Component c,                    \
        const fmi2ValueReference vr[], size_t nvr, ValueType value[]) {        \
        return callModel(c, __func__, [&](fmi2::Fmi2Base& model) {             \
            return model.GetMethod(vr, nvr, value);                            \
        });                                                                    \
    }                                                                          \
    fmi2Status FMI2_CALL fmi2Set##TypeName(fmi2Component c,                    \
        const fmi2ValueReference vr[], size_t nvr, const ValueType value[]) {  \
        return callModel(c, __func__, [&](fmi2::Fmi2Base& model) {             \
            return model.SetMethod(vr, nvr, value);                            \
        });                                                                    \
    }

FMI2_DEFINE_VALUE_ACCESSORS(Real, fmi2Real, getReal, setReal)
FMI2_DEFINE_VALUE_ACCESSORS(Integer, fmi2Integer, getInteger, setInteger)
FMI2_DEFINE_VALUE_ACCESSORS(Boolean, fmi2Boolean, getBoolean, setBoolean)
FMI2_DEFINE_VALUE_ACCESSORS(String, fmi2String, getString, setString)

#undef FMI2_DEFINE_VALUE_ACCESSORS

fmi2Status FMI2_CALL fmi2GetFMUstate(fmi2Component c, fmi2FMUstate* fmu_state) {
    return callModel(c, __func__,
        [&](fmi2::Fmi2Base& model) { return model.getFmuState(fmu_state); });
}

fmi2Status FMI2_CALL fmi2SetFMUstate(fmi2Component c, fmi2FMUstate fmu_state) {
    return callModel(c, __func__,
        [&](fmi2::Fmi2Base& model) { return model.setFmuState(fmu_state); });
}

fmi2Status FMI2_CALL fmi2FreeFMUstate(
    fmi2Component c, fmi2FMUstate* fmu_state) {
    return callModel(c, __func__,
        [&](fmi2::Fmi2Base& model) { return model.freeFmuState(fmu_state); });
}

fmi2Status FMI2_CALL fmi2SerializedFMUstateSize(
    fmi2Component c, fmi2FMUstate fmu_state, size_t* size) {
    return callModel(c, __func__, [&](fmi2::Fmi2Base& model) {
        return model.serializedFmuStateSize(fmu_state, size);
    });
}

fmi2Status FMI2_CALL fmi2SerializeFMUstate(fmi2Component c,
    fmi2FMUstate fmu_state, fmi2Byte serializedState[], size_t size) {
    return callModel(c, __func__, [&](fmi2::Fmi2Base& model) {
        return model.serializeFmuState(fmu_state, serializedState, size);
    });
}

fmi2Status FMI2_CALL fmi2DeSerializeFMUstate(fmi2Component c,
    const fmi2Byte serializedState[], size_t size, fmi2FMUstate* fmu_state) {
    return callModel(c, __func__, [&](fmi2::Fmi2Base& model) {
        return model.deserializeFmuState(serializedState, size, fmu_state);
    });
}

fmi2Status FMI2_CALL fmi2GetDirectionalDerivative(fmi2Component c,
    const fmi2ValueReference vUnknown_ref[], size_t nUnknown,
    const fmi2ValueReference vKnown_ref[], size_t nKnown,
    const fmi2Real dvKnown[], fmi2Real dvUnknown[]) {
    return callModel(c, __func__, [&](fmi2::Fmi2Base& model) {
        return model.getDirectionalDerivative(
            vUnknown_ref, nUnknown, vKnown_ref, nKnown, dvKnown, dvUnknown);
    });
}

fmi2Status FMI2_CALL fmi2SetTime(fmi2Component c, fmi2Real time) {
    return callMode<fmi2::MeBase>(
        c, __func__, [&](fmi2::MeBase& model) { return model.setTime(time); });
}

fmi2Status FMI2_CALL fmi2SetContinuousStates(
    fmi2Component c, const fmi2Real x[], size_t nx) {
    return callMode<fmi2::MeBase>(c, __func__,
        [&](fmi2::MeBase& model) { return model.setContinuousStates(x, nx); });
}

fmi2Status FMI2_CALL fmi2EnterEventMode(fmi2Component c) {
    return callMode<fmi2::MeBase>(c, __func__,
        [](fmi2::MeBase& model) { return model.enterEventMode(); });
}

fmi2Status FMI2_CALL fmi2NewDiscreteStates(
    fmi2Component c, fmi2EventInfo* eventInfo) {
    return callMode<fmi2::MeBase>(c, __func__, [&](fmi2::MeBase& model) {
        return model.newDiscreteStates(eventInfo);
    });
}

fmi2Status FMI2_CALL fmi2EnterContinuousTimeMode(fmi2Component c) {
    return callMode<fmi2::MeBase>(c, __func__,
        [](fmi2::MeBase& model) { return model.enterContinuousTimeMode(); });
}

fmi2Status FMI2_CALL fmi2CompletedIntegratorStep(fmi2Component c,
    fmi2Boolean no_set_fmu_state_prior_to_current_point,
    fmi2Boolean* enterEventMode, fmi2Boolean* terminateSimulation) {
    return callMode<fmi2::MeBase>(c, __func__, [&](fmi2::MeBase& model) {
        return model.completedIntegratorStep(
            no_set_fmu_state_prior_to_current_point, enterEventMode,
            terminateSimulation);
    });
}

fmi2Status FMI2_CALL fmi2GetDerivatives(
    fmi2Component c, fmi2Real derivatives[], size_t nx) {
    return callMode<fmi2::MeBase>(c, __func__, [&](fmi2::MeBase& model) {
        return model.getDerivatives(derivatives, nx);
    });
}

fmi2Status FMI2_CALL fmi2GetContinuousStates(
    fmi2Component c, fmi2Real x[], size_t nx) {
    return callMode<fmi2::MeBase>(c, __func__,
        [&](fmi2::MeBase& model) { return model.getContinuousStates(x, nx); });
}

fmi2Status FMI2_CALL fmi2GetEventIndicators(
    fmi2Component c, fmi2Real eventIndicators[], size_t ni) {
    return callMode<fmi2::MeBase>(c, __func__, [&](fmi2::MeBase& model) {
        return model.getEventIndicators(eventIndicators, ni);
    });
}

fmi2Status FMI2_CALL fmi2GetNominalsOfContinuousStates(
    fmi2Component c, fmi2Real x_nominal[], size_t nx) {
    return callMode<fmi2::MeBase>(c, __func__, [&](fmi2::MeBase& model) {
        return model.getNominalsOfContinuousStates(x_nominal, nx);
    });
}

fmi2Status FMI2_CALL fmi2DoStep(fmi2Component c,
    fmi2Real current_communication_point, fmi2Real communication_step_size,
    fmi2Boolean no_set_fmu_state_prior_to_current_point) {
    return callMode<fmi2::CsBase>(c, __func__, [&](fmi2::CsBase& model) {
        return model.doStep(current_communication_point,
            communication_step_size, no_set_fmu_state_prior_to_current_point);
    });
}

fmi2Status FMI2_CALL fmi2CancelStep(fmi2Component c) {
    return callMode<fmi2::CsBase>(
        c, __func__, [](fmi2::CsBase& model) { return model.cancelStep(); });
}

fmi2Status FMI2_CALL fmi2GetStatus(
    fmi2Component c, const fmi2StatusKind s, fmi2Status* value) {
    return callMode<fmi2::CsBase>(c, __func__,
        [&](fmi2::CsBase& model) { return model.getStatus(s, value); });
}

fmi2Status FMI2_CALL fmi2GetRealStatus(
    fmi2Component c, const fmi2StatusKind s, fmi2Real* value) {
    return callMode<fmi2::CsBase>(c, __func__,
        [&](fmi2::CsBase& model) { return model.getRealStatus(s, value); });
}

fmi2Status FMI2_CALL fmi2GetIntegerStatus(
    fmi2Component c, const fmi2StatusKind s, fmi2Integer* value) {
    return callMode<fmi2::CsBase>(c, __func__,
        [&](fmi2::CsBase& model) { return model.getIntegerStatus(s, value); });
}

fmi2Status FMI2_CALL fmi2GetBooleanStatus(
    fmi2Component c, const fmi2StatusKind s, fmi2Boolean* value) {
    return callMode<fmi2::CsBase>(c, __func__,
        [&](fmi2::CsBase& model) { return model.getBooleanStatus(s, value); });
}

fmi2Status FMI2_CALL fmi2GetStringStatus(
    fmi2Component c, const fmi2StatusKind s, fmi2String* value) {
    return callMode<fmi2::CsBase>(c, __func__,
        [&](fmi2::CsBase& model) { return model.getStringStatus(s, value); });
}

fmi2Status FMI2_CALL fmi2SetRealInputDerivatives(fmi2Component c,
    const fmi2ValueReference vr[], size_t nvr, const fmi2Integer order[],
    const fmi2Real value[]) {
    return callMode<fmi2::CsBase>(c, __func__, [&](fmi2::CsBase& model) {
        return model.setRealInputDerivatives(vr, nvr, order, value);
    });
}

fmi2Status FMI2_CALL fmi2GetRealOutputDerivatives(fmi2Component c,
    const fmi2ValueReference vr[], size_t nvr, const fmi2Integer order[],
    fmi2Real value[]) {
    return callMode<fmi2::CsBase>(c, __func__, [&](fmi2::CsBase& model) {
        return model.getRealOutputDerivatives(vr, nvr, order, value);
    });
}

}  // extern "C"
