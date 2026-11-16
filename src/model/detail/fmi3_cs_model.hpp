#ifndef FMI_MODEL_DETAIL_FMI3_CS_MODEL_HPP_
#define FMI_MODEL_DETAIL_FMI3_CS_MODEL_HPP_

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <filesystem>
#include <limits>

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
        this->state_ = State::kInstantiated;
        return fmi3OK;
    }
    fmi3Status setFloat64(const fmi3ValueReference references[],
        std::size_t count, const fmi3Float64 values[],
        std::size_t value_count) override {
        if (this->state_ == State::kTerminated || count != value_count)
            return fmi3Error;
        return result(this->computation_.set(
            references, count, values, this->state_ != State::kStep));
    }
    fmi3Status getFloat64(const fmi3ValueReference references[],
        std::size_t count, fmi3Float64 values[],
        std::size_t value_count) override {
        if (this->state_ == State::kTerminated || count != value_count)
            return fmi3Error;
        return result(this->computation_.get(references, count, values));
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
            (this->stop_defined_ && next - this->stop_time_ > tolerance))
            return fmi3Error;
        if (!this->computation_.step()) return fmi3Error;
        this->time_ = next;
        *last_time = next;
        return fmi3OK;
    }
    const char* lastError() const noexcept override {
        return this->computation_.lastError().c_str();
    }
    void clearError() override { this->computation_.clearError(); }

    private:
    static fmi3Status result(bool success) {
        return success ? fmi3OK : fmi3Error;
    }
    enum class State { kInstantiated, kInitialization, kStep, kTerminated };

    Computation computation_;
    State state_ = State::kInstantiated;
    double time_ = 0.0;
    double stop_time_ = 0.0;
    bool stop_defined_ = false;
};

}  // namespace model::detail

#endif  // FMI_MODEL_DETAIL_FMI3_CS_MODEL_HPP_
