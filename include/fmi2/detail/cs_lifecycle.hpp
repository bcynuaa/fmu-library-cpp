#ifndef FMI2_DETAIL_CS_LIFECYCLE_HPP_
#define FMI2_DETAIL_CS_LIFECYCLE_HPP_

#include <algorithm>
#include <cmath>
#include <limits>

#include "fmi2/fmi2_declare.h"

namespace fmi2 {
namespace detail {

// Lifecycle and communication time for synchronous Co-Simulation models.
// Models commit initialization and time only after their computation succeeds.
class CSLifecycle {
    public:
    fmi2Status setupExperiment(fmi2Boolean tolerance_defined,
        fmi2Real tolerance, fmi2Real start_time, fmi2Boolean stop_time_defined,
        fmi2Real stop_time) {
        if (this->state_ != State::kInstantiated ||
            !std::isfinite(start_time) ||
            (tolerance_defined &&
                (!std::isfinite(tolerance) || tolerance <= 0.0)) ||
            (stop_time_defined &&
                (!std::isfinite(stop_time) || stop_time <= start_time))) {
            return fmi2Error;
        }
        this->time_ = start_time;
        this->stop_time_ = stop_time_defined ? stop_time : 0.0;
        this->stop_time_defined_ = stop_time_defined;
        return fmi2OK;
    }

    fmi2Status enterInitializationMode() {
        if (this->state_ != State::kInstantiated) return fmi2Error;
        this->state_ = State::kInitialization;
        return fmi2OK;
    }

    bool isInitializing() const noexcept {
        return this->state_ == State::kInitialization;
    }

    void completeInitialization() noexcept { this->state_ = State::kStep; }

    fmi2Status terminate() {
        if (this->state_ != State::kStep) return fmi2Error;
        this->state_ = State::kTerminated;
        return fmi2OK;
    }

    void reset() noexcept {
        this->time_ = 0.0;
        this->stop_time_ = 0.0;
        this->stop_time_defined_ = false;
        this->fixed_step_size_ = 0.0;
        this->fixed_step_size_defined_ = false;
        this->state_ = State::kInstantiated;
    }

    bool isTerminated() const noexcept {
        return this->state_ == State::kTerminated;
    }

    bool canSetInitialValues() const noexcept {
        return this->state_ == State::kInstantiated || isInitializing();
    }

    bool canStep(fmi2Real current_communication_point,
        fmi2Real communication_step_size,
        bool supports_variable_step_size) const noexcept {
        const double next_time =
            current_communication_point + communication_step_size;
        const double time_tolerance = 8.0 *
            std::numeric_limits<double>::epsilon() *
            (std::max)(1.0, std::abs(this->time_));
        return this->state_ == State::kStep &&
            std::isfinite(current_communication_point) &&
            std::isfinite(communication_step_size) &&
            communication_step_size > 0.0 && std::isfinite(next_time) &&
            next_time > current_communication_point &&
            next_time > this->time_ &&
            std::abs(current_communication_point - this->time_) <=
            time_tolerance &&
            (supports_variable_step_size || !this->fixed_step_size_defined_ ||
                std::abs(communication_step_size - this->fixed_step_size_) <=
                    8.0 * std::numeric_limits<double>::epsilon() *
                        (std::max)(1.0, std::abs(this->fixed_step_size_))) &&
            (!this->stop_time_defined_ ||
                next_time - this->stop_time_ <= time_tolerance);
    }

    void completeStep(fmi2Real current_communication_point,
        fmi2Real communication_step_size) noexcept {
        this->time_ = current_communication_point + communication_step_size;
        if (!this->fixed_step_size_defined_) {
            this->fixed_step_size_ = communication_step_size;
            this->fixed_step_size_defined_ = true;
        }
    }

    private:
    enum class State { kInstantiated, kInitialization, kStep, kTerminated };

    State state_ = State::kInstantiated;
    double time_ = 0.0;
    double stop_time_ = 0.0;
    bool stop_time_defined_ = false;
    double fixed_step_size_ = 0.0;
    bool fixed_step_size_defined_ = false;
};

}  // namespace detail
}  // namespace fmi2

#endif  // FMI2_DETAIL_CS_LIFECYCLE_HPP_
