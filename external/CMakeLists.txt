# Third-party code. Submodules only, never edited in place.

# Vela: rendering. Its examples and install rules aren't needed here.
set(VELA_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(VELA_INSTALL OFF CACHE BOOL "" FORCE)
add_subdirectory(Vela)

# Dear ImGui: dev tools only. Rendered through Vela by Engine/src/Debug.
if(LOCKSTEP_DEV_TOOLS)
    add_library(imgui STATIC
        imgui/imgui.cpp
        imgui/imgui_draw.cpp
        imgui/imgui_tables.cpp
        imgui/imgui_widgets.cpp
        imgui/imgui_demo.cpp
    )

    target_include_directories(imgui SYSTEM PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/imgui)
endif()

# GameNetworkingSockets: transport, wrapped privately by Engine/src/Net. Static, OpenSSL crypto,
# protobuf from the system. No P2P (ICE/WebRTC), examples, tests or tools.
set(BUILD_STATIC_LIB ON CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIB OFF CACHE BOOL "" FORCE)
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(BUILD_TOOLS OFF CACHE BOOL "" FORCE)
set(ENABLE_ICE OFF CACHE BOOL "" FORCE)
set(USE_STEAMWEBRTC OFF CACHE BOOL "" FORCE)
set(USE_CRYPTO "OpenSSL" CACHE STRING "" FORCE)
add_subdirectory(GameNetworkingSockets SYSTEM)

# PhysX: prebuilt once by external/physx/scripts into external/physx/lib/<os>/<type> (same setup
# as Velix). Exposes the INTERFACE target `physx`; on Windows also PHYSX_RUNTIME_DLLS.
add_subdirectory(physx)

# cgltf: glTF 2.0 parser (header-only), used privately by Engine/src/Assets.
add_library(cgltf INTERFACE)
target_include_directories(cgltf SYSTEM INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/cgltf)

# KTX-Software: KTX2 container, Zstandard and Basis Universal transcoding, used privately by
# Engine/src/Assets through the static read-only library `ktx_read`. No tools, tests or GPU upload.
block()
    set(BUILD_SHARED_LIBS OFF)
    set(KTX_FEATURE_TESTS OFF CACHE BOOL "" FORCE)
    set(KTX_FEATURE_TOOLS OFF CACHE BOOL "" FORCE)
    set(KTX_FEATURE_DOC OFF CACHE BOOL "" FORCE)
    set(KTX_FEATURE_ETC_UNPACK OFF CACHE BOOL "" FORCE)
    set(KTX_FEATURE_GL_UPLOAD OFF CACHE BOOL "" FORCE)
    set(KTX_FEATURE_VK_UPLOAD OFF CACHE BOOL "" FORCE)
    add_subdirectory(KTX-Software SYSTEM EXCLUDE_FROM_ALL)
endblock()

# Basis Universal transcoding is ~10x slower unoptimised; keep it optimised in Debug builds so
# large KTX2 scenes still load in reasonable time.
if(NOT MSVC)
    target_compile_options(ktx_read PRIVATE $<$<CONFIG:Debug>:-O2>)
endif()
