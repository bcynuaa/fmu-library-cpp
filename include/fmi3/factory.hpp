#ifndef FMI3_FACTORY_HPP_
#define FMI3_FACTORY_HPP_

#include "fmi3/fmi3_base.hpp"

namespace fmi3 {

// Defined by exactly one concrete FMU model translation unit. Return a heap
// instance of the requested interface, or nullptr when unsupported. The C
// wrapper owns the instance and releases it through fmi3FreeInstance.
Fmi3Base* createFmi3Instance(InterfaceType type);

}  // namespace fmi3

#endif  // FMI3_FACTORY_HPP_
