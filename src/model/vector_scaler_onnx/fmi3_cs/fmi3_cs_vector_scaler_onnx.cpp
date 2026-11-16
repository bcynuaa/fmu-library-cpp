#include "../../detail/fmi3_cs_model.hpp"
#include "../model.hpp"
#include "fmi3/factory.hpp"

namespace fmi3 {

Fmi3Base* createFmi3Instance(InterfaceType type) {
    if (type == InterfaceType::kCoSimulation) {
        return new model::detail::FMI3CSModel<vector_scaler_onnx::Model>();
    }
    return nullptr;
}

}  // namespace fmi3
