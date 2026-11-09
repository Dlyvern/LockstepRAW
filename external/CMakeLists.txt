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
