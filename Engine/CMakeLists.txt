add_library(LockstepEngine INTERFACE)
add_library(Lockstep::Engine ALIAS LockstepEngine)

target_link_libraries(LockstepEngine INTERFACE Vela::Builtins Vela::Vela)