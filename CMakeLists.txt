cmake_minimum_required(VERSION 4.0)

project(OrgEngineV2 LANGUAGES C CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 23)

# --- Output Directory Configuration ---
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/App)

# Force MSVC to dump all executables into the base App folder
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/App)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/App)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO ${CMAKE_BINARY_DIR}/App)

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
        ${CORE_DIR}/Renderer/RenderEnums.h
        ${CORE_DIR}/Renderer/RenderResources.h
        ${CORE_DIR}/Tools/DDSHeader.h
        ${CORE_DIR}/Tools/JSONParser.h
        ${CORE_DIR}/Tools/JsonParser.cpp
        ${CORE_DIR}/Tools/TOMLParser.cpp
        ${CORE_DIR}/Tools/TOMLParser.h
        ${ENGINE_DIR}/BindlessManager.cpp
        ${CORE_DIR}/Tools/EnumBitmask.h
        ${GAME_DIR}/ComputeDemonstration.h
        ${GAME_DIR}/ComputeDemonstration.cpp
        ${CORE_DIR}/Renderer/Vulkan/VulkanImGuiHelpets.h
        ${CORE_DIR}/Tools/Span.h
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
            -g
            ${ENTRY_POINTS}
            -emit-spirv-directly
            -fvk-use-scalar-layout
            -fvk-use-entrypoint-name
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
add_subdirectory(${3RDPARTY_DIR}/fastgltf)

option(TRACY_ENABLE "" ON)
option(TRACY_ON_DEMAND "" ON)
set(TRACY_CALLSTACK 6 CACHE STRING "Tracy callstack depth")
add_subdirectory(${3RDPARTY_DIR}/tracy)

add_library(ImGui STATIC
        ${3RDPARTY_DIR}/imgui/imgui.cpp
        ${3RDPARTY_DIR}/imgui/imgui_demo.cpp
        ${3RDPARTY_DIR}/imgui/imgui_draw.cpp
        ${3RDPARTY_DIR}/imgui/imgui_tables.cpp
        ${3RDPARTY_DIR}/imgui/imgui_widgets.cpp
        ${3RDPARTY_DIR}/imgui/backends/imgui_impl_vulkan.cpp
)

target_include_directories(ImGui PUBLIC
        ${3RDPARTY_DIR}/imgui
        ${3RDPARTY_DIR}/imgui/backends
)

target_compile_definitions(ImGui PRIVATE IMGUI_IMPL_VULKAN_USE_VOLK)
target_link_libraries(ImGui PRIVATE volk)
set_target_properties(ImGui PROPERTIES CXX_CLANG_TIDY "")

# --- Main Executable Target ---
add_executable(OrgEngineV2 ${SRC_FILES})
add_dependencies(OrgEngineV2 CompileShaders)
target_precompile_headers(OrgEngineV2 PRIVATE ${CMAKE_SOURCE_DIR}/Source/PrimTypes.h)

# --- Core Target Configuration ---
target_compile_definitions(OrgEngineV2 PRIVATE
        UNICODE
        VK_NO_PROTOTYPES
        VOLK_STATIC_DEFINES
        VMA_VULKAN_HEADERS_ALREADY_INCLUDED
        NOMINMAX
        EDITORUI
        VULKAN_BUILD
        _CRT_SECURE_NO_WARNINGS
        ENABLE_GPU_TIMING
        ENGINE_COMMIT_HASH="${GIT_HASH}"
        ENGINE_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
        $<$<NOT:$<CONFIG:Release>>:ENGINE_ENABLE_LOGGING>

        $<$<NOT:$<CONFIG:Release>>:VULKAN_DEBUG_MODE>
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
        ${3RDPARTY_DIR}/imoguizmo
        ${3RDPARTY_DIR}/mikktspace
        ${3RDPARTY_DIR}/slang/include
)

target_link_libraries(OrgEngineV2 PRIVATE
        fmt::fmt
        volk
        ImGui
        GPUOpen::VulkanMemoryAllocator
        meshoptimizer
        mikktspace
        glm
        ufbx
        Tracy::TracyClient
        FMOD::FMOD
        fastgltf::fastgltf
        ${3RDPARTY_DIR}/slang/lib/slang.lib
)

# --- Target Optimizations (Multi-Config Safe) ---
if(MSVC)
    target_compile_options(OrgEngineV2 PRIVATE
            /MP /Zc:__cplusplus /permissive- /W4
    )
else()
    target_compile_options(OrgEngineV2 PRIVATE
            -Wall -Wextra -Wpedantic -Wshadow
    )
endif()

set_property(TARGET OrgEngineV2 PROPERTY INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE)
set_property(TARGET OrgEngineV2 PROPERTY INTERPROCEDURAL_OPTIMIZATION_RELWITHDEBINFO TRUE)

# --- Platform Specific Block ---
if (ENGINE_PLATFORM STREQUAL "SDL")
    target_compile_definitions(OrgEngineV2 PRIVATE ENGINE_PLATFORM_SDL)
    target_sources(OrgEngineV2 PRIVATE
            ${CORE_DIR}/Platform/PlatformSDL.cpp
            ${CORE_DIR}/Input/InputSystemSDL.cpp
    )

    target_sources(ImGui PRIVATE ${3RDPARTY_DIR}/imgui/backends/imgui_impl_sdl3.cpp)
    add_subdirectory(${3RDPARTY_DIR}/sdl EXCLUDE_FROM_ALL)
    target_link_libraries(OrgEngineV2 PRIVATE SDL3::SDL3)
else() # WIN32 Platform
    target_compile_definitions(OrgEngineV2 PRIVATE ENGINE_PLATFORM_WIN32 VK_USE_PLATFORM_WIN32_KHR)
    target_sources(OrgEngineV2 PRIVATE
            ${CORE_DIR}/Platform/PlatformWindows.cpp
            ${CORE_DIR}/Input/InputSysGameInput.cpp
    )
    target_sources(ImGui PRIVATE ${3RDPARTY_DIR}/imgui/backends/imgui_impl_win32.cpp)
    target_include_directories(OrgEngineV2 PRIVATE ${3RDPARTY_DIR}/gameinput/include)
    target_link_directories(OrgEngineV2 PRIVATE ${3RDPARTY_DIR}/gameinput/lib)
    target_link_libraries(OrgEngineV2 PRIVATE ${3RDPARTY_DIR}/gameinput/lib/x64/GameInput.lib)

    if (MSVC)
        target_link_options(OrgEngineV2 PRIVATE
                $<$<CONFIG:Release>:/SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup>
                $<$<NOT:$<CONFIG:Release>>:/SUBSYSTEM:CONSOLE>
        )
    endif ()
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