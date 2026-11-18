#ifndef FMI_MODEL_DETAIL_FMI3_CS_MODEL_HPP_
#define FMI_MODEL_DETAIL_FMI3_CS_MODEL_HPP_

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <filesystem>
#include <limits>
#include <string>
#include <vector>

#include "fmi3/cs_base.hpp"

namespace model::detail {

template <typename Computation>
class FMI3CSModel final : public fmi3::CsBase {
    public:
    fmi3Status instantiate(fmi3String, fmi3String, fmi3String resources,
        fmi3Boolean, fmi3Boolean, fmi3Boolean event_mode_used, fmi3Boolean,
        const fmi3ValueReference[], std::size_t intermediate_count,
        fmi3InstanceEnvironment, fmi3LogMessageCallback,
        fmi3IntermediateUpdateCallback) override {
        if (event_mode_used || intermediate_count != 0) return fmi3Error;
        // * A missing resource path is legal for FMI 3; the model decides
        // * whether it can work without resources.
        const std::filesystem::path path = resources && resources[0]
            ? std::filesystem::u8path(resources)
            : std::filesystem::path{};
        return result(this->computation_.load(path));
    }

    fmi3Status enterInitializationMode(fmi3Boolean tolerance_defined,
        fmi3Float64 tolerance, fmi3Float64 start, fmi3Boolean stop_defined,
        fmi3Float64 stop) override {
        if (this->state_ != State::kInstantiated || !std::isfinite(start) ||
            (tolerance_defined &&
                (!std::isfinite(tolerance) || tolerance <= 0.0)) ||
            (stop_defined && (!std::isfinite(stop) || stop <= start)))
            return fmi3Error;
        this->time_ = start;
        this->stop_time_ = stop_defined ? stop : 0.0;
        this->stop_defined_ = stop_defined;
        this->state_ = State::kInitialization;
        return fmi3OK;
    }
    fmi3Status exitInitializationMode() override {
        if (this->state_ != State::kInitialization) return fmi3Error;
        if (!this->computation_.initialize()) return fmi3Error;
        this->state_ = State::kStep;
        return fmi3OK;
    }
    fmi3Status terminate() override {
        if (this->state_ != State::kStep) return fmi3Error;
        this->state_ = State::kTerminated;
        return fmi3OK;
    }
    fmi3Status reset() override {
        this->computation_.reset();
        this->time_ = 0.0;
        this->stop_time_ = 0.0;
        this->stop_defined_ = false;
        this->fixed_step_size_ = 0.0;
        this->fixed_step_size_defined_ = false;
        this->state_ = State::kInstantiated;
        return fmi3OK;
    }
    fmi3Status setFloat64(const fmi3ValueReference references[],
        std::size_t count, const fmi3Float64 values[],
        std::size_t value_count) override {
        if (this->state_ == State::kTerminated || count != value_count ||
            (count > 0 && (!references || !values)))
            return this->fail("Invalid Float64 write request");
        const auto time_reference =
            this->computation_.valueReferenceCount() + 1;
        for (std::size_t index = 0; index < count; ++index) {
            if (references[index] == time_reference)
                return this->fail("The independent time variable is read-only");
        }
        return result(this->computation_.set(
            references, count, values, this->state_ != State::kStep));
    }
    fmi3Status getFloat64(const fmi3ValueReference references[],
        std::size_t count, fmi3Float64 values[],
        std::size_t value_count) override {
        if (this->state_ == State::kTerminated || count != value_count ||
            (count > 0 && (!references || !values)))
            return this->fail("Invalid Float64 read request");
        const auto time_reference =
            this->computation_.valueReferenceCount() + 1;
        std::vector<fmi3ValueReference> model_references;
        std::vector<std::size_t> model_positions;
        std::vector<fmi3Float64> result_values(count);
        for (std::size_t index = 0; index < count; ++index) {
            if (references[index] == time_reference) {
                result_values[index] = this->time_;
            } else if (references[index] < time_reference) {
                model_references.push_back(references[index]);
                model_positions.push_back(index);
            } else {
                return this->fail("Invalid valueReference " +
                    std::to_string(references[index]));
            }
        }
        if (!model_references.empty()) {
            std::vector<fmi3Float64> model_values(model_references.size());
            if (!this->computation_.get(model_references.data(),
                    model_references.size(), model_values.data()))
                return fmi3Error;
            for (std::size_t index = 0; index < model_values.size(); ++index)
                result_values[model_positions[index]] = model_values[index];
        }
        if (count > 0)
            std::copy(result_values.begin(), result_values.end(), values);
        return fmi3OK;
    }
    fmi3Status doStep(fmi3Float64 point, fmi3Float64 step_size, fmi3Boolean,
        fmi3Boolean* event_needed, fmi3Boolean* terminate_simulation,
        fmi3Boolean* early_return, fmi3Float64* last_time) override {
        if (!event_needed || !terminate_simulation || !early_return ||
            !last_time)
            return fmi3Error;
        *event_needed = false;
        *terminate_simulation = false;
        *early_return = false;
        *last_time = this->time_;
        const double next = point + step_size;
        const double tolerance = 8.0 * std::numeric_limits<double>::epsilon() *
            (std::max)(1.0, std::abs(this->time_));
        if (this->state_ != State::kStep || !std::isfinite(point) ||
            !std::isfinite(step_size) || step_size <= 0.0 ||
            !std::isfinite(next) || next <= point || next <= this->time_ ||
            std::abs(point - this->time_) > tolerance ||
            (!this->computation_.supportsVariableCommunicationStepSize() &&
                this->fixed_step_size_defined_ &&
                std::abs(step_size - this->fixed_step_size_) > 8.0 *
                        std::numeric_limits<double>::epsilon() *
                        (std::max)(1.0, std::abs(this->fixed_step_size_))) ||
            (this->stop_defined_ && next - this->stop_time_ > tolerance))
            return fmi3Error;
        if (!this->computation_.step()) return fmi3Error;
        this->time_ = next;
        if (!this->computation_.supportsVariableCommunicationStepSize()) {
            this->fixed_step_size_ = step_size;
            this->fixed_step_size_defined_ = true;
        }
        *last_time = next;
        return fmi3OK;
    }
    const char* lastError() const noexcept override {
        if (!this->error_.empty()) return this->error_.c_str();
        return this->computation_.lastError().c_str();
    }
    void clearError() override {
        this->error_.clear();
        this->computation_.clearError();
    }

    private:
    fmi3Status fail(const std::string& message) {
        this->error_ = message;
        return fmi3Error;
    }
    static fmi3Status result(bool success) {
        return success ? fmi3OK : fmi3Error;
    }
    enum class State { kInstantiated, kInitialization, kStep, kTerminated };

    Computation computation_;
    std::string error_;
    State state_ = State::kInstantiated;
    double time_ = 0.0;
    double stop_time_ = 0.0;
    bool stop_defined_ = false;
    double fixed_step_size_ = 0.0;
    bool fixed_step_size_defined_ = false;
};

}  // namespace model::detail

#endif  // FMI_MODEL_DETAIL_FMI3_CS_MODEL_HPP_
