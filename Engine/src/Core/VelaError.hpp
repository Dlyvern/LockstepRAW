#ifndef LOCKSTEP_CORE_VELA_ERROR_HPP
#define LOCKSTEP_CORE_VELA_ERROR_HPP

#include "Lockstep/Core/Result.hpp"

#include <Vela/Result.hpp>

namespace lockstep
{
    // Translates a Vela error into an Engine error, keeping the message. Engine-internal: every
    // module that calls Vela uses it.
    // error:   the Vela error.
    // Returns: the equivalent Engine error.
    [[nodiscard]] Error toError(const vela::Error& error);
} //namespace lockstep

#endif //LOCKSTEP_CORE_VELA_ERROR_HPP
