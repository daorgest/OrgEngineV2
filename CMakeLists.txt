cmake_minimum_required(VERSION 4.0)

project(OrgEngineV2 LANGUAGES C CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 23)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# --- Output Directory Configuration ---
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/App)
if(MSVC)
    foreach(OUTPUTCONFIG ${CMAKE_CONFIGURATION_TYPES})
        string(TOUPPER ${OUTPUTCONFIG} OUTPUTCONFIG)
        set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${OUTPUTCONFIG} ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})
        set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_${OUTPUTCONFIG} ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})
        set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_${OUTPUTCONFIG} ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})
    endforeach()
endif()

# --- Path Definitions ---
set(3RDPARTY_DIR  ${CMAKE_SOURCE_DIR}/3rdParty)
set(CORE_DIR      ${CMAKE_SOURCE_DIR}/Source/Core)
set(ENGINE_DIR    ${CMAKE_SOURCE_DIR}/Source/Engine)
set(GAME_DIR      ${CMAKE_SOURCE_DIR}/Source/Game)
set(TOOLS_DIR     ${CMAKE_SOURCE_DIR}/Source/Core/Tools)

# --- Versioning (with GitHub ZIP fallback) ---
# If a user downloads the repo as a ZIP instead of 'git clone', git rev-parse fails.
execute_process(
        COMMAND git rev-parse --short HEAD
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        OUTPUT_VARIABLE GIT_HASH
        RESULT_VARIABLE GIT_RESULT
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
)
if(NOT GIT_RESULT EQUAL 0)
    set(GIT_HASH "zip-download")
endif()

# --- Platform Selection ---
set(ENGINE_PLATFORM "WIN32" CACHE STRING "Platform backend")
set_property(CACHE ENGINE_PLATFORM PROPERTY STRINGS WIN32 SDL)

# --- Source Lists ---
set(SRC_FILES
        Source/main.cpp
        ${TOOLS_DIR}/Arena.cpp
        ${TOOLS_DIR}/MemStuff.cpp
        ${TOOLS_DIR}/Logger.cpp
        ${CORE_DIR}/Renderer/Vulkan/VulkanInit.cpp
        ${CORE_DIR}/Renderer/Vulkan/VulkanDevice.cpp
        ${CORE_DIR}/Renderer/Vulkan/VmaUsage.cpp
        ${CORE_DIR}/Renderer/Vulkan/VulkanSwapchain.cpp
        ${CORE_DIR}/Renderer/Vulkan/VulkanTexture.cpp
        ${CORE_DIR}/Renderer/Vulkan/VulkanCommands.cpp
        ${CORE_DIR}/Renderer/Vulkan/VulkanCommandBuffer.cpp
        ${CORE_DIR}/Renderer/Vulkan/VulkanPipeline.cpp
        ${CORE_DIR}/Renderer/Vulkan/VulkanShader.cpp
        ${CORE_DIR}/Renderer/Vulkan/VulkanBuffer.cpp
        ${CORE_DIR}/Renderer/Vulkan/VulkanDescriptors.cpp
        ${CORE_DIR}/Renderer/Vulkan/VulkanShaderBuffer.cpp
        ${CORE_DIR}/Renderer/Vulkan/VulkanQueryPool.cpp
        ${CORE_DIR}/Renderer/Vulkan/VulkanMesh.cpp
        ${CORE_DIR}/Renderer/Vulkan/VulkanShaderManager.cpp
        ${CORE_DIR}/Renderer/Vulkan/VulkanShaderManager.h
        ${CORE_DIR}/Math/MikkWrapper.cpp
        ${CORE_DIR}/Input/InputSys.cpp
        ${CORE_DIR}/Audio/AudioFMOD.cpp
        ${ENGINE_DIR}/TextureLoader.cpp
        ${ENGINE_DIR}/MeshLoader.cpp
        ${ENGINE_DIR}/Camera.cpp
        ${ENGINE_DIR}/EditorUi.cpp
        ${ENGINE_DIR}/FPSCamera.cpp
        ${ENGINE_DIR}/ShaderCompiler.h
        ${ENGINE_DIR}/ShaderCompiler.cpp
        ${ENGINE_DIR}/BindlessManager.h
        ${ENGINE_DIR}/Ray.h
        ${GAME_DIR}/Application.cpp
        ${GAME_DIR}/DebugRenderer.cpp
        ${GAME_DIR}/SceneRenderer.cpp
        ${GAME_DIR}/SkyboxManager.cpp
)

set(IMGUI_FILES
        ${3RDPARTY_DIR}/imgui/imgui.cpp
        ${3RDPARTY_DIR}/imgui/imgui_demo.cpp
        ${3RDPARTY_DIR}/imgui/imgui_draw.cpp
        ${3RDPARTY_DIR}/imgui/imgui_tables.cpp
        ${3RDPARTY_DIR}/imgui/imgui_widgets.cpp
        ${3RDPARTY_DIR}/imgui/backends/imgui_impl_vulkan.cpp
)

# --- Shader Compilation Logic ---
file(GLOB_RECURSE SLANG_ALL CONFIGURE_DEPENDS "${GAME_DIR}/Shaders/*.slang")

foreach(shader ${SLANG_ALL})
    get_filename_component(name "${shader}" NAME_WE)
    set(out "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/Shaders/${name}.spv")

    set(ENTRY_POINTS "")
    if(shader MATCHES "(_comp|\\.comp)\\.slang$")
        list(APPEND ENTRY_POINTS -entry compMain)
    else()
        list(APPEND ENTRY_POINTS -entry vertexMain -entry fragmentMain)
    endif()

    add_custom_command(
            OUTPUT "${out}"
            COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/Shaders"
            COMMAND "${3RDPARTY_DIR}/slang/bin/slangc.exe"
            "${shader}"
            -o "${out}"
            -target spirv
            -profile sm_6_6
            ${ENTRY_POINTS}
            -emit-spirv-directly
            -fvk-use-scalar-layout
            -matrix-layout-column-major
            DEPENDS "${shader}" "${3RDPARTY_DIR}/slang/bin/slangc.exe"
            VERBATIM
    )
    list(APPEND COMPILED_SHADERS "${out}")
endforeach()
add_custom_target(CompileShaders ALL DEPENDS ${COMPILED_SHADERS})

# --- 3rd Party Subdirectories ---
add_subdirectory(${3RDPARTY_DIR}/fmt)
add_subdirectory(${3RDPARTY_DIR}/volk)
add_subdirectory(${3RDPARTY_DIR}/vma)
add_subdirectory(${3RDPARTY_DIR}/meshoptimizer)
add_subdirectory(${3RDPARTY_DIR}/mikktspace)
add_subdirectory(${3RDPARTY_DIR}/glm)
add_subdirectory(${3RDPARTY_DIR}/ufbx)
add_subdirectory(${3RDPARTY_DIR}/fmod)

option(TRACY_ENABLE "" OFF)
option(TRACY_ON_DEMAND "" ON)
option(TRACY_CALLSTACK "" 6)
add_subdirectory(${3RDPARTY_DIR}/tracy)

# --- Main Executable Target ---
add_executable(OrgEngineV2 ${SRC_FILES} ${IMGUI_FILES})
add_dependencies(OrgEngineV2 CompileShaders)
target_precompile_headers(OrgEngineV2 PRIVATE ${CMAKE_SOURCE_DIR}/Source/PrimTypes.h)

# --- Core Target Configuration ---
target_compile_definitions(OrgEngineV2 PRIVATE
        UNICODE
        VK_NO_PROTOTYPES
        VOLK_STATIC_DEFINES
        NOMINMAX
        EDITORUI
        VULKAN_BUILD
        IMGUI_IMPL_VULKAN_USE_VOLK
        _CRT_SECURE_NO_WARNINGS
        ENABLE_GPU_TIMING
        ENGINE_COMMIT_HASH="${GIT_HASH}"
        ENGINE_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
        # Conditional Debug Definitions
        $<$<NOT:$<CONFIG:Release>>:VULKAN_DEBUG_MODE ENGINE_ENABLE_LOGGING>
)

target_include_directories(OrgEngineV2 PRIVATE
        ${CORE_DIR}
        ${CORE_DIR}/Platform
        ${CORE_DIR}/Renderer
        ${CORE_DIR}/Renderer/Vulkan
        ${CORE_DIR}/Math
        ${ENGINE_DIR}
        ${GAME_DIR}
)

target_include_directories(OrgEngineV2 SYSTEM PRIVATE
        ${3RDPARTY_DIR}/stb
        ${3RDPARTY_DIR}/tinyobjloader
        ${3RDPARTY_DIR}/imgui
        ${3RDPARTY_DIR}/imgui/backends
        ${3RDPARTY_DIR}/imoguizmo
        ${3RDPARTY_DIR}/fmt/include
        ${3RDPARTY_DIR}/volk
        ${3RDPARTY_DIR}/vma/include
        ${3RDPARTY_DIR}/meshoptimizer/src
        ${3RDPARTY_DIR}/mikktspace
        ${3RDPARTY_DIR}/glm
        ${3RDPARTY_DIR}/ufbx
        ${3RDPARTY_DIR}/tracy/public
        ${3RDPARTY_DIR}/slang/include
)

target_link_libraries(OrgEngineV2 PRIVATE
        fmt::fmt
        volk
        GPUOpen::VulkanMemoryAllocator
        meshoptimizer
        mikktspace
        glm
        ufbx
        Tracy::TracyClient
        FMOD::FMOD
        ${3RDPARTY_DIR}/slang/lib/slang.lib
)

# --- Target Optimizations (Multi-Config Safe) ---
if(MSVC)
    target_compile_options(OrgEngineV2 PRIVATE
            /MP /Zc:__cplusplus /permissive- /W4

            # Release specific:
            $<$<CONFIG:Release>:/O2 /Oi /Ot /GL /Gy /Gw /Zc:inline>
    )
    target_link_options(OrgEngineV2 PRIVATE
            $<$<CONFIG:Release>:/LTCG /OPT:REF /OPT:ICF>
            $<$<CONFIG:Debug>:/DEBUG:FASTLINK /INCREMENTAL>
            $<$<CONFIG:RelWithDebInfo>:/DEBUG /INCREMENTAL>
    )
    # Target-specific C++ Flags appending
    set_property(TARGET OrgEngineV2 APPEND_STRING PROPERTY COMPILE_FLAGS_DEBUG " /RTC1 /MDd")
else()
    target_compile_options(OrgEngineV2 PRIVATE
            -Wall -Wextra -Wpedantic -Wshadow
            # Release specific:
            $<$<CONFIG:Release>:-O3 -flto -fno-plt -ffunction-sections -fdata-sections>
    )
    target_link_options(OrgEngineV2 PRIVATE
            $<$<CONFIG:Release>:-Wl,--gc-sections>
    )
endif()

# --- Platform Specific Block ---
if (ENGINE_PLATFORM STREQUAL "SDL")
    target_compile_definitions(OrgEngineV2 PRIVATE ENGINE_PLATFORM_SDL)
    target_sources(OrgEngineV2 PRIVATE
            ${CORE_DIR}/Platform/PlatformSDL.cpp
            ${CORE_DIR}/Input/InputSystemSDL.cpp
            ${3RDPARTY_DIR}/imgui/backends/imgui_impl_sdl3.cpp
    )
    add_subdirectory(${3RDPARTY_DIR}/sdl EXCLUDE_FROM_ALL)
    target_link_libraries(OrgEngineV2 PRIVATE SDL3::SDL3)

    # Copy SDL3 DLL to output dir
    add_custom_command(TARGET OrgEngineV2 POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:SDL3::SDL3>
            "$<TARGET_FILE_DIR:OrgEngineV2>/"
            COMMENT "Copying SDL3 DLL..."
    )
else() # WIN32 Platform
    target_compile_definitions(OrgEngineV2 PRIVATE ENGINE_PLATFORM_WIN32 VK_USE_PLATFORM_WIN32_KHR)
    target_sources(OrgEngineV2 PRIVATE
            ${CORE_DIR}/Platform/PlatformWindows.cpp
            ${CORE_DIR}/Input/InputSysGameInput.cpp
            ${3RDPARTY_DIR}/imgui/backends/imgui_impl_win32.cpp
    )
    target_include_directories(OrgEngineV2 PRIVATE ${3RDPARTY_DIR}/gameinput/include)
    target_link_directories(OrgEngineV2 PRIVATE ${3RDPARTY_DIR}/gameinput/lib)
    target_link_libraries(OrgEngineV2 PRIVATE ${3RDPARTY_DIR}/gameinput/lib/x64/GameInput.lib)

    set_target_properties(OrgEngineV2 PROPERTIES
            WIN32_EXECUTABLE $<$<OR:$<CONFIG:Release>,$<CONFIG:RelWithDebInfo>>:TRUE>
    )
endif()

# --- Post-Build Steps (Copy Assets, Slang, & FMOD) ---

add_custom_command(TARGET OrgEngineV2 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/Assets"
        "$<TARGET_FILE_DIR:OrgEngineV2>/Assets"
        COMMENT "Copying Assets to output directory..."
        VERBATIM
)
add_custom_command(TARGET OrgEngineV2 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${3RDPARTY_DIR}/slang/bin/slang.dll"
        "$<TARGET_FILE_DIR:OrgEngineV2>/"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${3RDPARTY_DIR}/slang/bin/slang-compiler.dll"
        "$<TARGET_FILE_DIR:OrgEngineV2>/"
        COMMENT "Copying Slang DLLs to output directory..."
        VERBATIM
)

if(COMMAND fmod_copy_dlls)
    fmod_copy_dlls(OrgEngineV2)
endif()