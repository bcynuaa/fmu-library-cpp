#ifndef FMI2_FACTORY_HPP_
#define FMI2_FACTORY_HPP_

#include "fmi2/fmi2_base.hpp"

namespace fmi2 {

// Defined by exactly one concrete FMU model translation unit. The factory
// returns an instance whose type matches the requested FMI interface.
Fmi2Base* createFmi2Instance(fmi2Type type);

}  // namespace fmi2

#endif  // FMI2_FACTORY_HPP_
