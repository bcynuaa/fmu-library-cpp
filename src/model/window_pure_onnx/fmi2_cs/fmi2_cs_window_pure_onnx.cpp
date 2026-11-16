#include "../../detail/fmi2_cs_model.hpp"
#include "../model.hpp"
#include "fmi2/factory.hpp"

namespace fmi2 {

Fmi2Base* createFmi2Instance(fmi2Type type) {
    if (type == fmi2CoSimulation) {
        return new model::detail::FMI2CSModel<window_pure_onnx::Model>();
    }
    return nullptr;
}

}  // namespace fmi2
