#ifndef LOCKSTEP_CORE_VELA_LOG_HPP
#define LOCKSTEP_CORE_VELA_LOG_HPP

namespace lockstep
{
    // Routes Vela's log messages into the Engine log (channel "Vela") and drops Vela's
    // per-object debug chatter. Validation warnings and errors stay visible. Safe to call more
    // than once; Engine-internal, called before the first Vela object is created.
    void routeVelaLogs();
} //namespace lockstep

#endif //LOCKSTEP_CORE_VELA_LOG_HPP
