#ifndef FMI2_DECLARE_H_
#define FMI2_DECLARE_H_

#include <stddef.h>

#include "fmi2/fmi2_platform.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void* fmi2Component;
typedef void* fmi2ComponentEnvironment;
typedef void* fmi2FMUstate;
typedef unsigned int fmi2ValueReference;
typedef int fmi2Integer;
typedef double fmi2Real;
typedef int fmi2Boolean;
typedef char fmi2Char;
typedef const fmi2Char* fmi2String;
typedef char fmi2Byte;

#define fmi2True 1
#define fmi2False 0

typedef enum { fmi2ModelExchange, fmi2CoSimulation } fmi2Type;

typedef enum {
    fmi2OK,
    fmi2Warning,
    fmi2Discard,
    fmi2Error,
    fmi2Fatal,
    fmi2Pending
} fmi2Status;

typedef enum {
    fmi2DoStepStatus,
    fmi2PendingStatus,
    fmi2LastSuccessfulTime,
    fmi2Terminated
} fmi2StatusKind;

typedef struct {
    fmi2Boolean newDiscreteStatesNeeded;
    fmi2Boolean terminateSimulation;
    fmi2Boolean nominalsOfContinuousStatesChanged;
    fmi2Boolean valuesOfContinuousStatesChanged;
    fmi2Boolean nextEventTimeDefined;
    fmi2Real nextEventTime;
} fmi2EventInfo;

typedef void(FMI2_CALL* fmi2CallbackLogger)(
    fmi2ComponentEnvironment componentEnvironment, fmi2String instanceName,
    fmi2Status status, fmi2String category, fmi2String message, ...);

typedef void*(FMI2_CALL* fmi2CallbackAllocateMemory)(size_t nobj, size_t size);
typedef void(FMI2_CALL* fmi2CallbackFreeMemory)(void* obj);
typedef void(FMI2_CALL* fmi2CallbackStepFinished)(
    fmi2ComponentEnvironment componentEnvironment, fmi2Status status);

// Kept as a source-compatible alias for the name used by older revisions.
typedef fmi2CallbackStepFinished fmi2StepFinished;

typedef struct {
    fmi2CallbackLogger logger;
    fmi2CallbackAllocateMemory allocateMemory;
    fmi2CallbackFreeMemory freeMemory;
    fmi2CallbackStepFinished stepFinished;
    fmi2ComponentEnvironment componentEnvironment;
} fmi2CallbackFunctions;

FMI2_EXPORT fmi2String FMI2_CALL fmi2GetTypesPlatform(void);
FMI2_EXPORT fmi2String FMI2_CALL fmi2GetVersion(void);
FMI2_EXPORT fmi2Status FMI2_CALL fmi2SetDebugLogging(fmi2Component c,
    fmi2Boolean loggingOn, size_t nCategories, const fmi2String categories[]);
FMI2_EXPORT fmi2Component FMI2_CALL fmi2Instantiate(fmi2String instanceName,
    fmi2Type fmuType, fmi2String fmuGUID, fmi2String fmuResourceLocation,
    const fmi2CallbackFunctions* functions, fmi2Boolean visible,
    fmi2Boolean loggingOn);
FMI2_EXPORT void FMI2_CALL fmi2FreeInstance(fmi2Component c);
FMI2_EXPORT fmi2Status FMI2_CALL fmi2SetupExperiment(fmi2Component c,
    fmi2Boolean toleranceDefined, fmi2Real tolerance, fmi2Real startTime,
    fmi2Boolean stopTimeDefined, fmi2Real stopTime);
FMI2_EXPORT fmi2Status FMI2_CALL fmi2EnterInitializationMode(fmi2Component c);
FMI2_EXPORT fmi2Status FMI2_CALL fmi2ExitInitializationMode(fmi2Component c);
FMI2_EXPORT fmi2Status FMI2_CALL fmi2Terminate(fmi2Component c);
FMI2_EXPORT fmi2Status FMI2_CALL fmi2Reset(fmi2Component c);

FMI2_EXPORT fmi2Status FMI2_CALL fmi2GetReal(fmi2Component c,
    const fmi2ValueReference vr[], size_t nvr, fmi2Real value[]);
FMI2_EXPORT fmi2Status FMI2_CALL fmi2SetReal(fmi2Component c,
    const fmi2ValueReference vr[], size_t nvr, const fmi2Real value[]);
FMI2_EXPORT fmi2Status FMI2_CALL fmi2GetInteger(fmi2Component c,
    const fmi2ValueReference vr[], size_t nvr, fmi2Integer value[]);
FMI2_EXPORT fmi2Status FMI2_CALL fmi2SetInteger(fmi2Component c,
    const fmi2ValueReference vr[], size_t nvr, const fmi2Integer value[]);
FMI2_EXPORT fmi2Status FMI2_CALL fmi2GetBoolean(fmi2Component c,
    const fmi2ValueReference vr[], size_t nvr, fmi2Boolean value[]);
FMI2_EXPORT fmi2Status FMI2_CALL fmi2SetBoolean(fmi2Component c,
    const fmi2ValueReference vr[], size_t nvr, const fmi2Boolean value[]);
FMI2_EXPORT fmi2Status FMI2_CALL fmi2GetString(fmi2Component c,
    const fmi2ValueReference vr[], size_t nvr, fmi2String value[]);
FMI2_EXPORT fmi2Status FMI2_CALL fmi2SetString(fmi2Component c,
    const fmi2ValueReference vr[], size_t nvr, const fmi2String value[]);

FMI2_EXPORT fmi2Status FMI2_CALL fmi2GetFMUstate(
    fmi2Component c, fmi2FMUstate* FMUstate);
FMI2_EXPORT fmi2Status FMI2_CALL fmi2SetFMUstate(
    fmi2Component c, fmi2FMUstate FMUstate);
FMI2_EXPORT fmi2Status FMI2_CALL fmi2FreeFMUstate(
    fmi2Component c, fmi2FMUstate* FMUstate);
FMI2_EXPORT fmi2Status FMI2_CALL fmi2SerializedFMUstateSize(
    fmi2Component c, fmi2FMUstate FMUstate, size_t* size);
FMI2_EXPORT fmi2Status FMI2_CALL fmi2SerializeFMUstate(fmi2Component c,
    fmi2FMUstate FMUstate, fmi2Byte serializedState[], size_t size);
FMI2_EXPORT fmi2Status FMI2_CALL fmi2DeSerializeFMUstate(fmi2Component c,
    const fmi2Byte serializedState[], size_t size, fmi2FMUstate* FMUstate);
FMI2_EXPORT fmi2Status FMI2_CALL fmi2GetDirectionalDerivative(fmi2Component c,
    const fmi2ValueReference vUnknown_ref[], size_t nUnknown,
    const fmi2ValueReference vKnown_ref[], size_t nKnown,
    const fmi2Real dvKnown[], fmi2Real dvUnknown[]);

FMI2_EXPORT fmi2Status FMI2_CALL fmi2SetTime(fmi2Component c, fmi2Real time);
FMI2_EXPORT fmi2Status FMI2_CALL fmi2SetContinuousStates(
    fmi2Component c, const fmi2Real x[], size_t nx);
FMI2_EXPORT fmi2Status FMI2_CALL fmi2EnterEventMode(fmi2Component c);
FMI2_EXPORT fmi2Status FMI2_CALL fmi2NewDiscreteStates(
    fmi2Component c, fmi2EventInfo* eventInfo);
FMI2_EXPORT fmi2Status FMI2_CALL fmi2EnterContinuousTimeMode(fmi2Component c);
FMI2_EXPORT fmi2Status FMI2_CALL fmi2CompletedIntegratorStep(fmi2Component c,
    fmi2Boolean noSetFMUStatePriorToCurrentPoint, fmi2Boolean* enterEventMode,
    fmi2Boolean* terminateSimulation);
FMI2_EXPORT fmi2Status FMI2_CALL fmi2GetDerivatives(
    fmi2Component c, fmi2Real derivatives[], size_t nx);
FMI2_EXPORT fmi2Status FMI2_CALL fmi2GetContinuousStates(
    fmi2Component c, fmi2Real x[], size_t nx);
FMI2_EXPORT fmi2Status FMI2_CALL fmi2GetEventIndicators(
    fmi2Component c, fmi2Real eventIndicators[], size_t ni);
FMI2_EXPORT fmi2Status FMI2_CALL fmi2GetNominalsOfContinuousStates(
    fmi2Component c, fmi2Real x_nominal[], size_t nx);

FMI2_EXPORT fmi2Status FMI2_CALL fmi2DoStep(fmi2Component c,
    fmi2Real currentCommunicationPoint, fmi2Real communicationStepSize,
    fmi2Boolean noSetFMUStatePriorToCurrentPoint);
FMI2_EXPORT fmi2Status FMI2_CALL fmi2CancelStep(fmi2Component c);
FMI2_EXPORT fmi2Status FMI2_CALL fmi2GetStatus(
    fmi2Component c, const fmi2StatusKind s, fmi2Status* value);
FMI2_EXPORT fmi2Status FMI2_CALL fmi2GetRealStatus(
    fmi2Component c, const fmi2StatusKind s, fmi2Real* value);
FMI2_EXPORT fmi2Status FMI2_CALL fmi2GetIntegerStatus(
    fmi2Component c, const fmi2StatusKind s, fmi2Integer* value);
FMI2_EXPORT fmi2Status FMI2_CALL fmi2GetBooleanStatus(
    fmi2Component c, const fmi2StatusKind s, fmi2Boolean* value);
FMI2_EXPORT fmi2Status FMI2_CALL fmi2GetStringStatus(
    fmi2Component c, const fmi2StatusKind s, fmi2String* value);
FMI2_EXPORT fmi2Status FMI2_CALL fmi2SetRealInputDerivatives(fmi2Component c,
    const fmi2ValueReference vr[], size_t nvr, const fmi2Integer order[],
    const fmi2Real value[]);
FMI2_EXPORT fmi2Status FMI2_CALL fmi2GetRealOutputDerivatives(fmi2Component c,
    const fmi2ValueReference vr[], size_t nvr, const fmi2Integer order[],
    fmi2Real value[]);

#ifdef __cplusplus
}
#endif

#endif  // FMI2_DECLARE_H_
