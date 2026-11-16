#ifndef FMI3_DECLARE_H_
#define FMI3_DECLARE_H_

/*
FMI 3.0 C API declarations, adapted from the FMI headers.

Copyright (C) 2008-2011 MODELISAR consortium,
              2012-2023 Modelica Association Project "FMI"
              All rights reserved.

This file is licensed by the copyright holders under the 2-Clause BSD License
(https://opensource.org/licenses/BSD-2-Clause):

----------------------------------------------------------------------------
Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

- Redistributions of source code must retain the above copyright notice,
 this list of conditions and the following disclaimer.

- Redistributions in binary form must reproduce the above copyright notice,
 this list of conditions and the following disclaimer in the documentation
 and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
----------------------------------------------------------------------------
*/

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "fmi3/fmi3_platform.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void* fmi3Instance;
typedef void* fmi3InstanceEnvironment;
typedef void* fmi3FMUState;
typedef uint32_t fmi3ValueReference;
typedef float fmi3Float32;
typedef double fmi3Float64;
typedef int8_t fmi3Int8;
typedef uint8_t fmi3UInt8;
typedef int16_t fmi3Int16;
typedef uint16_t fmi3UInt16;
typedef int32_t fmi3Int32;
typedef uint32_t fmi3UInt32;
typedef int64_t fmi3Int64;
typedef uint64_t fmi3UInt64;
typedef bool fmi3Boolean;
typedef char fmi3Char;
typedef const fmi3Char* fmi3String;
typedef uint8_t fmi3Byte;
typedef const fmi3Byte* fmi3Binary;
typedef bool fmi3Clock;
#define fmi3True true
#define fmi3False false
#define fmi3ClockActive true
#define fmi3ClockInactive false

#define fmi3Version "3.0"

typedef enum {
    fmi3OK,
    fmi3Warning,
    fmi3Discard,
    fmi3Error,
    fmi3Fatal,
} fmi3Status;

typedef enum {
    fmi3Independent,
    fmi3Constant,
    fmi3Fixed,
    fmi3Tunable,
    fmi3Discrete,
    fmi3Dependent
} fmi3DependencyKind;

typedef enum {
    fmi3IntervalNotYetKnown,
    fmi3IntervalUnchanged,
    fmi3IntervalChanged
} fmi3IntervalQualifier;

typedef void (*fmi3LogMessageCallback)(
    fmi3InstanceEnvironment instance_environment, fmi3Status status,
    fmi3String category, fmi3String message);

typedef void (*fmi3ClockUpdateCallback)(
    fmi3InstanceEnvironment instance_environment);

typedef void (*fmi3IntermediateUpdateCallback)(
    fmi3InstanceEnvironment instance_environment,
    fmi3Float64 intermediate_update_time,
    fmi3Boolean intermediate_variable_set_requested,
    fmi3Boolean intermediate_variable_get_allowed,
    fmi3Boolean intermediate_step_finished, fmi3Boolean can_return_early,
    fmi3Boolean* early_return_requested, fmi3Float64* early_return_time);

typedef void (*fmi3LockPreemptionCallback)(void);
typedef void (*fmi3UnlockPreemptionCallback)(void);

// Common and Co-Simulation entry points.

FMI3_EXPORT const char* FMI3_CALL fmi3GetVersion(void);

FMI3_EXPORT fmi3Status FMI3_CALL fmi3SetDebugLogging(fmi3Instance instance,
    fmi3Boolean logging_on, size_t n_categories, const fmi3String categories[]);

FMI3_EXPORT fmi3Instance FMI3_CALL fmi3InstantiateCoSimulation(
    fmi3String instance_name, fmi3String instantiation_token,
    fmi3String resource_path, fmi3Boolean visible, fmi3Boolean logging_on,
    fmi3Boolean event_mode_used, fmi3Boolean early_return_allowed,
    const fmi3ValueReference required_intermediate_variables[],
    size_t n_required_intermediate_variables,
    fmi3InstanceEnvironment instance_environment,
    fmi3LogMessageCallback log_message,
    fmi3IntermediateUpdateCallback intermediate_update);

FMI3_EXPORT void FMI3_CALL fmi3FreeInstance(fmi3Instance instance);

FMI3_EXPORT fmi3Status FMI3_CALL fmi3EnterInitializationMode(
    fmi3Instance instance, fmi3Boolean tolerance_defined, fmi3Float64 tolerance,
    fmi3Float64 start_time, fmi3Boolean stop_time_defined,
    fmi3Float64 stop_time);

FMI3_EXPORT fmi3Status FMI3_CALL fmi3ExitInitializationMode(
    fmi3Instance instance);

FMI3_EXPORT fmi3Status FMI3_CALL fmi3EnterEventMode(fmi3Instance instance);

FMI3_EXPORT fmi3Status FMI3_CALL fmi3Terminate(fmi3Instance instance);

FMI3_EXPORT fmi3Status FMI3_CALL fmi3Reset(fmi3Instance instance);

FMI3_EXPORT fmi3Status FMI3_CALL fmi3GetFloat32(fmi3Instance instance,
    const fmi3ValueReference value_references[], size_t n_value_references,
    fmi3Float32 values[], size_t n_values);

FMI3_EXPORT fmi3Status FMI3_CALL fmi3GetFloat64(fmi3Instance instance,
    const fmi3ValueReference value_references[], size_t n_value_references,
    fmi3Float64 values[], size_t n_values);

FMI3_EXPORT fmi3Status FMI3_CALL fmi3GetInt8(fmi3Instance instance,
    const fmi3ValueReference value_references[], size_t n_value_references,
    fmi3Int8 values[], size_t n_values);

FMI3_EXPORT fmi3Status FMI3_CALL fmi3GetUInt8(fmi3Instance instance,
    const fmi3ValueReference value_references[], size_t n_value_references,
    fmi3UInt8 values[], size_t n_values);

FMI3_EXPORT fmi3Status FMI3_CALL fmi3GetInt16(fmi3Instance instance,
    const fmi3ValueReference value_references[], size_t n_value_references,
    fmi3Int16 values[], size_t n_values);

FMI3_EXPORT fmi3Status FMI3_CALL fmi3GetUInt16(fmi3Instance instance,
    const fmi3ValueReference value_references[], size_t n_value_references,
    fmi3UInt16 values[], size_t n_values);

FMI3_EXPORT fmi3Status FMI3_CALL fmi3GetInt32(fmi3Instance instance,
    const fmi3ValueReference value_references[], size_t n_value_references,
    fmi3Int32 values[], size_t n_values);

FMI3_EXPORT fmi3Status FMI3_CALL fmi3GetUInt32(fmi3Instance instance,
    const fmi3ValueReference value_references[], size_t n_value_references,
    fmi3UInt32 values[], size_t n_values);

FMI3_EXPORT fmi3Status FMI3_CALL fmi3GetInt64(fmi3Instance instance,
    const fmi3ValueReference value_references[], size_t n_value_references,
    fmi3Int64 values[], size_t n_values);

FMI3_EXPORT fmi3Status FMI3_CALL fmi3GetUInt64(fmi3Instance instance,
    const fmi3ValueReference value_references[], size_t n_value_references,
    fmi3UInt64 values[], size_t n_values);

FMI3_EXPORT fmi3Status FMI3_CALL fmi3GetBoolean(fmi3Instance instance,
    const fmi3ValueReference value_references[], size_t n_value_references,
    fmi3Boolean values[], size_t n_values);

FMI3_EXPORT fmi3Status FMI3_CALL fmi3GetString(fmi3Instance instance,
    const fmi3ValueReference value_references[], size_t n_value_references,
    fmi3String values[], size_t n_values);

FMI3_EXPORT fmi3Status FMI3_CALL fmi3GetBinary(fmi3Instance instance,
    const fmi3ValueReference value_references[], size_t n_value_references,
    size_t value_sizes[], fmi3Binary values[], size_t n_values);

FMI3_EXPORT fmi3Status FMI3_CALL fmi3GetClock(fmi3Instance instance,
    const fmi3ValueReference value_references[], size_t n_value_references,
    fmi3Clock values[]);

FMI3_EXPORT fmi3Status FMI3_CALL fmi3SetFloat32(fmi3Instance instance,
    const fmi3ValueReference value_references[], size_t n_value_references,
    const fmi3Float32 values[], size_t n_values);

FMI3_EXPORT fmi3Status FMI3_CALL fmi3SetFloat64(fmi3Instance instance,
    const fmi3ValueReference value_references[], size_t n_value_references,
    const fmi3Float64 values[], size_t n_values);

FMI3_EXPORT fmi3Status FMI3_CALL fmi3SetInt8(fmi3Instance instance,
    const fmi3ValueReference value_references[], size_t n_value_references,
    const fmi3Int8 values[], size_t n_values);

FMI3_EXPORT fmi3Status FMI3_CALL fmi3SetUInt8(fmi3Instance instance,
    const fmi3ValueReference value_references[], size_t n_value_references,
    const fmi3UInt8 values[], size_t n_values);

FMI3_EXPORT fmi3Status FMI3_CALL fmi3SetInt16(fmi3Instance instance,
    const fmi3ValueReference value_references[], size_t n_value_references,
    const fmi3Int16 values[], size_t n_values);

FMI3_EXPORT fmi3Status FMI3_CALL fmi3SetUInt16(fmi3Instance instance,
    const fmi3ValueReference value_references[], size_t n_value_references,
    const fmi3UInt16 values[], size_t n_values);

FMI3_EXPORT fmi3Status FMI3_CALL fmi3SetInt32(fmi3Instance instance,
    const fmi3ValueReference value_references[], size_t n_value_references,
    const fmi3Int32 values[], size_t n_values);

FMI3_EXPORT fmi3Status FMI3_CALL fmi3SetUInt32(fmi3Instance instance,
    const fmi3ValueReference value_references[], size_t n_value_references,
    const fmi3UInt32 values[], size_t n_values);

FMI3_EXPORT fmi3Status FMI3_CALL fmi3SetInt64(fmi3Instance instance,
    const fmi3ValueReference value_references[], size_t n_value_references,
    const fmi3Int64 values[], size_t n_values);

FMI3_EXPORT fmi3Status FMI3_CALL fmi3SetUInt64(fmi3Instance instance,
    const fmi3ValueReference value_references[], size_t n_value_references,
    const fmi3UInt64 values[], size_t n_values);

FMI3_EXPORT fmi3Status FMI3_CALL fmi3SetBoolean(fmi3Instance instance,
    const fmi3ValueReference value_references[], size_t n_value_references,
    const fmi3Boolean values[], size_t n_values);

FMI3_EXPORT fmi3Status FMI3_CALL fmi3SetString(fmi3Instance instance,
    const fmi3ValueReference value_references[], size_t n_value_references,
    const fmi3String values[], size_t n_values);

FMI3_EXPORT fmi3Status FMI3_CALL fmi3SetBinary(fmi3Instance instance,
    const fmi3ValueReference value_references[], size_t n_value_references,
    const size_t value_sizes[], const fmi3Binary values[], size_t n_values);

FMI3_EXPORT fmi3Status FMI3_CALL fmi3SetClock(fmi3Instance instance,
    const fmi3ValueReference value_references[], size_t n_value_references,
    const fmi3Clock values[]);

FMI3_EXPORT fmi3Status FMI3_CALL fmi3GetNumberOfVariableDependencies(
    fmi3Instance instance, fmi3ValueReference value_reference,
    size_t* n_dependencies);

FMI3_EXPORT fmi3Status FMI3_CALL fmi3GetVariableDependencies(
    fmi3Instance instance, fmi3ValueReference dependent,
    size_t element_indices_of_dependent[], fmi3ValueReference independents[],
    size_t element_indices_of_independents[],
    fmi3DependencyKind dependency_kinds[], size_t n_dependencies);

FMI3_EXPORT fmi3Status FMI3_CALL fmi3GetFMUState(
    fmi3Instance instance, fmi3FMUState* fmu_state);

FMI3_EXPORT fmi3Status FMI3_CALL fmi3SetFMUState(
    fmi3Instance instance, fmi3FMUState fmu_state);

FMI3_EXPORT fmi3Status FMI3_CALL fmi3FreeFMUState(
    fmi3Instance instance, fmi3FMUState* fmu_state);

FMI3_EXPORT fmi3Status FMI3_CALL fmi3SerializedFMUStateSize(
    fmi3Instance instance, fmi3FMUState fmu_state, size_t* size);

FMI3_EXPORT fmi3Status FMI3_CALL fmi3SerializeFMUState(fmi3Instance instance,
    fmi3FMUState fmu_state, fmi3Byte serialized_state[], size_t size);

FMI3_EXPORT fmi3Status FMI3_CALL fmi3DeserializeFMUState(fmi3Instance instance,
    const fmi3Byte serialized_state[], size_t size, fmi3FMUState* fmu_state);

FMI3_EXPORT fmi3Status FMI3_CALL fmi3GetDirectionalDerivative(
    fmi3Instance instance, const fmi3ValueReference unknowns[],
    size_t n_unknowns, const fmi3ValueReference knowns[], size_t n_knowns,
    const fmi3Float64 seed[], size_t n_seed, fmi3Float64 sensitivity[],
    size_t n_sensitivity);

FMI3_EXPORT fmi3Status FMI3_CALL fmi3GetAdjointDerivative(fmi3Instance instance,
    const fmi3ValueReference unknowns[], size_t n_unknowns,
    const fmi3ValueReference knowns[], size_t n_knowns,
    const fmi3Float64 seed[], size_t n_seed, fmi3Float64 sensitivity[],
    size_t n_sensitivity);

FMI3_EXPORT fmi3Status FMI3_CALL fmi3EnterConfigurationMode(
    fmi3Instance instance);

FMI3_EXPORT fmi3Status FMI3_CALL fmi3ExitConfigurationMode(
    fmi3Instance instance);

FMI3_EXPORT fmi3Status FMI3_CALL fmi3GetIntervalDecimal(fmi3Instance instance,
    const fmi3ValueReference value_references[], size_t n_value_references,
    fmi3Float64 intervals[], fmi3IntervalQualifier qualifiers[]);

FMI3_EXPORT fmi3Status FMI3_CALL fmi3GetIntervalFraction(fmi3Instance instance,
    const fmi3ValueReference value_references[], size_t n_value_references,
    fmi3UInt64 counters[], fmi3UInt64 resolutions[],
    fmi3IntervalQualifier qualifiers[]);

FMI3_EXPORT fmi3Status FMI3_CALL fmi3GetShiftDecimal(fmi3Instance instance,
    const fmi3ValueReference value_references[], size_t n_value_references,
    fmi3Float64 shifts[]);

FMI3_EXPORT fmi3Status FMI3_CALL fmi3GetShiftFraction(fmi3Instance instance,
    const fmi3ValueReference value_references[], size_t n_value_references,
    fmi3UInt64 counters[], fmi3UInt64 resolutions[]);

FMI3_EXPORT fmi3Status FMI3_CALL fmi3SetIntervalDecimal(fmi3Instance instance,
    const fmi3ValueReference value_references[], size_t n_value_references,
    const fmi3Float64 intervals[]);

FMI3_EXPORT fmi3Status FMI3_CALL fmi3SetIntervalFraction(fmi3Instance instance,
    const fmi3ValueReference value_references[], size_t n_value_references,
    const fmi3UInt64 counters[], const fmi3UInt64 resolutions[]);

FMI3_EXPORT fmi3Status FMI3_CALL fmi3SetShiftDecimal(fmi3Instance instance,
    const fmi3ValueReference value_references[], size_t n_value_references,
    const fmi3Float64 shifts[]);

FMI3_EXPORT fmi3Status FMI3_CALL fmi3SetShiftFraction(fmi3Instance instance,
    const fmi3ValueReference value_references[], size_t n_value_references,
    const fmi3UInt64 counters[], const fmi3UInt64 resolutions[]);

FMI3_EXPORT fmi3Status FMI3_CALL fmi3EvaluateDiscreteStates(
    fmi3Instance instance);

FMI3_EXPORT fmi3Status FMI3_CALL fmi3UpdateDiscreteStates(fmi3Instance instance,
    fmi3Boolean* discrete_states_need_update, fmi3Boolean* terminate_simulation,
    fmi3Boolean* nominals_of_continuous_states_changed,
    fmi3Boolean* values_of_continuous_states_changed,
    fmi3Boolean* next_event_time_defined, fmi3Float64* next_event_time);

FMI3_EXPORT fmi3Status FMI3_CALL fmi3EnterStepMode(fmi3Instance instance);

FMI3_EXPORT fmi3Status FMI3_CALL fmi3GetOutputDerivatives(fmi3Instance instance,
    const fmi3ValueReference value_references[], size_t n_value_references,
    const fmi3Int32 orders[], fmi3Float64 values[], size_t n_values);

FMI3_EXPORT fmi3Status FMI3_CALL fmi3DoStep(fmi3Instance instance,
    fmi3Float64 current_communication_point,
    fmi3Float64 communication_step_size,
    fmi3Boolean no_set_fmu_state_prior_to_current_point,
    fmi3Boolean* event_handling_needed, fmi3Boolean* terminate_simulation,
    fmi3Boolean* early_return, fmi3Float64* last_successful_time);

// Unsupported modes have exported failure stubs for importer compatibility.

FMI3_EXPORT fmi3Instance FMI3_CALL fmi3InstantiateModelExchange(
    fmi3String instance_name, fmi3String instantiation_token,
    fmi3String resource_path, fmi3Boolean visible, fmi3Boolean logging_on,
    fmi3InstanceEnvironment instance_environment,
    fmi3LogMessageCallback log_message);

FMI3_EXPORT fmi3Instance FMI3_CALL fmi3InstantiateScheduledExecution(
    fmi3String instance_name, fmi3String instantiation_token,
    fmi3String resource_path, fmi3Boolean visible, fmi3Boolean logging_on,
    fmi3InstanceEnvironment instance_environment,
    fmi3LogMessageCallback log_message, fmi3ClockUpdateCallback clock_update,
    fmi3LockPreemptionCallback lock_preemption,
    fmi3UnlockPreemptionCallback unlock_preemption);

FMI3_EXPORT fmi3Status FMI3_CALL fmi3EnterContinuousTimeMode(
    fmi3Instance instance);

FMI3_EXPORT fmi3Status FMI3_CALL fmi3CompletedIntegratorStep(
    fmi3Instance instance, fmi3Boolean no_set_fmu_state_prior_to_current_point,
    fmi3Boolean* enter_event_mode, fmi3Boolean* terminate_simulation);

FMI3_EXPORT fmi3Status FMI3_CALL fmi3SetTime(
    fmi3Instance instance, fmi3Float64 time);

FMI3_EXPORT fmi3Status FMI3_CALL fmi3SetContinuousStates(fmi3Instance instance,
    const fmi3Float64 continuous_states[], size_t n_continuous_states);

FMI3_EXPORT fmi3Status FMI3_CALL fmi3GetContinuousStateDerivatives(
    fmi3Instance instance, fmi3Float64 derivatives[],
    size_t n_continuous_states);

FMI3_EXPORT fmi3Status FMI3_CALL fmi3GetEventIndicators(fmi3Instance instance,
    fmi3Float64 event_indicators[], size_t n_event_indicators);

FMI3_EXPORT fmi3Status FMI3_CALL fmi3GetContinuousStates(fmi3Instance instance,
    fmi3Float64 continuous_states[], size_t n_continuous_states);

FMI3_EXPORT fmi3Status FMI3_CALL fmi3GetNominalsOfContinuousStates(
    fmi3Instance instance, fmi3Float64 nominals[], size_t n_continuous_states);

FMI3_EXPORT fmi3Status FMI3_CALL fmi3GetNumberOfEventIndicators(
    fmi3Instance instance, size_t* n_event_indicators);

FMI3_EXPORT fmi3Status FMI3_CALL fmi3GetNumberOfContinuousStates(
    fmi3Instance instance, size_t* n_continuous_states);

FMI3_EXPORT fmi3Status FMI3_CALL fmi3ActivateModelPartition(
    fmi3Instance instance, fmi3ValueReference clock_reference,
    fmi3Float64 activation_time);

#ifdef __cplusplus
}
#endif

#endif  // FMI3_DECLARE_H_
