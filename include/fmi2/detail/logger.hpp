#ifndef FMI2_DETAIL_LOGGER_HPP_
#define FMI2_DETAIL_LOGGER_HPP_

#include <string>

#include "fmi2/fmi2_declare.h"

namespace fmi2::detail {

class Logger {
    public:
    void configure(fmi2String name, const fmi2CallbackFunctions* callbacks) {
        this->name_ = name ? name : "";
        if (callbacks) this->callbacks_ = *callbacks;
    }

    void error(const char* operation, const char* reason) const noexcept {
        if (!this->callbacks_.logger) return;
        // Host callbacks must not let exceptions escape the C ABI.
        try {
            this->callbacks_.logger(this->callbacks_.componentEnvironment,
                this->name_.c_str(), fmi2Error, "logStatusError", "%s: %s",
                operation,
                reason && reason[0]
                    ? reason
                    : "Invalid arguments, lifecycle, or unsupported operation");
        } catch (...) {
        }
    }

    private:
    std::string name_;
    fmi2CallbackFunctions callbacks_{};
};

}  // namespace fmi2::detail

#endif  // FMI2_DETAIL_LOGGER_HPP_
