#ifndef FMI3_DETAIL_LOGGER_HPP_
#define FMI3_DETAIL_LOGGER_HPP_

#include <string>

#include "fmi3/fmi3_declare.h"

namespace fmi3::detail {

class Logger {
    public:
    void configure(fmi3InstanceEnvironment environment,
        fmi3LogMessageCallback callback) noexcept {
        this->environment_ = environment;
        this->callback_ = callback;
    }

    void error(const char* operation, const char* reason) const noexcept {
        if (!this->callback_) return;
        try {
            const std::string message = std::string(operation) + ": " +
                (reason && reason[0] ? reason
                                     : "Invalid arguments, lifecycle, or "
                                       "unsupported operation");
            this->callback_(this->environment_, fmi3Error, "logStatusError",
                message.c_str());
        } catch (...) {
        }
    }

    private:
    fmi3InstanceEnvironment environment_ = nullptr;
    fmi3LogMessageCallback callback_ = nullptr;
};

}  // namespace fmi3::detail

#endif  // FMI3_DETAIL_LOGGER_HPP_
