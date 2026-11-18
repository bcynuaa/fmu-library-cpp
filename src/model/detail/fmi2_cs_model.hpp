#ifndef FMI_MODEL_DETAIL_FMI2_CS_MODEL_HPP_
#define FMI_MODEL_DETAIL_FMI2_CS_MODEL_HPP_

#include <cstddef>

#include "fmi2/cs_base.hpp"
#include "fmi2/detail/cs_lifecycle.hpp"
#include "utils/fmu_io.hpp"

namespace model::detail {

template <typename Computation>
class FMI2CSModel final : public fmi2::CsBase {
    public:
    fmi2Status instantiate(fmi2String, fmi2Type type, fmi2String,
        fmi2String resources, const fmi2CallbackFunctions*, fmi2Boolean,
        fmi2Boolean) override {
        if (type != fmi2CoSimulation) return fmi2Error;
        return result(
            this->computation_.load(fmuio::resourceLocationToPath(resources)));
    }

    fmi2Status setupExperiment(fmi2Boolean tolerance_defined,
        fmi2Real tolerance, fmi2Real start, fmi2Boolean stop_defined,
        fmi2Real stop) override {
        return this->lifecycle_.setupExperiment(
            tolerance_defined, tolerance, start, stop_defined, stop);
    }
    fmi2Status enterInitializationMode() override {
        return this->lifecycle_.enterInitializationMode();
    }
    fmi2Status exitInitializationMode() override {
        if (!this->lifecycle_.isInitializing()) return fmi2Error;
        if (!this->computation_.initialize()) return fmi2Error;
        this->lifecycle_.completeInitialization();
        return fmi2OK;
    }
    fmi2Status terminate() override { return this->lifecycle_.terminate(); }
    fmi2Status reset() override {
        this->computation_.reset();
        this->lifecycle_.reset();
        return fmi2OK;
    }
    fmi2Status setReal(const fmi2ValueReference references[], std::size_t count,
        const fmi2Real values[]) override {
        if (this->lifecycle_.isTerminated()) return fmi2Error;
        return result(this->computation_.set(
            references, count, values, this->lifecycle_.canSetInitialValues()));
    }
    fmi2Status getReal(const fmi2ValueReference references[], std::size_t count,
        fmi2Real values[]) override {
        return result(this->computation_.get(references, count, values));
    }
    fmi2Status doStep(
        fmi2Real point, fmi2Real step_size, fmi2Boolean) override {
        if (!this->lifecycle_.canStep(point, step_size,
                this->computation_.supportsVariableCommunicationStepSize()))
            return fmi2Error;
        if (!this->computation_.step()) return fmi2Error;
        this->lifecycle_.completeStep(point, step_size);
        return fmi2OK;
    }
    const char* lastError() const noexcept override {
        return this->computation_.lastError().c_str();
    }
    void clearError() override { this->computation_.clearError(); }

    private:
    static fmi2Status result(bool success) {
        return success ? fmi2OK : fmi2Error;
    }

    Computation computation_;
    fmi2::detail::CSLifecycle lifecycle_;
};

}  // namespace model::detail

#endif  // FMI_MODEL_DETAIL_FMI2_CS_MODEL_HPP_
