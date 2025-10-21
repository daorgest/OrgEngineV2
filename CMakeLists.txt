cmake_minimum_required(VERSION 3.31)
project(OrgEngineV2)

# --- Dev toggles / quality-of-life -------------------------------------------------
option(ORGENGINE_WARN_AS_ERROR "Treat warnings as errors in CI" OFF)

# VSCode IntelliSense / clangd
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CORE_DIR      ${CMAKE_CURRENT_SOURCE_DIR}/Source/Core)
set(ENGINE_DIR    ${CMAKE_CURRENT_SOURCE_DIR}/Source/Engine)
set(GAME_DIR      ${CMAKE_CURRENT_SOURCE_DIR}/Source/Game)
set(TOOLS_DIR     ${CMAKE_CURRENT_SOURCE_DIR}/Source/Core/Tools)
set(RENDERER_DIR  ${CMAKE_CURRENT_SOURCE_DIR}/Source/Core/Renderer)
set(3RDPARTY_DIR  ${CMAKE_SOURCE_DIR}/3rdParty)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)
add_compile_definitions(
        UNICODE VK_NO_PROTOTYPES VOLK_STATIC_DEFINES VK_USE_PLATFORM_WIN32_KHR NOMINMAX EDITORUI
        IMGUI_IMPL_VULKAN_USE_VOLK _CRT_SECURE_NO_WARNINGS
#        VULKAN_DEBUG_MODE
#        ENABLE_TIMING
)

# --- Git Version Hash --------------------------------------------------------------
execute_process(
        COMMAND git rev-parse --short HEAD
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        OUTPUT_VARIABLE GIT_HASH
        OUTPUT_STRIP_TRAILING_WHITESPACE
)
add_compile_definitions(ENGINE_COMMIT_HASH="${GIT_HASH}")

# Set output folder for the executable
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/App)

# --- Per-config output (helps VS pick the right working dir) -----------------------
foreach(_cfg IN ITEMS Debug RelWithDebInfo Release MinSizeRel)
    string(TOUPPER "${_cfg}" _CFG)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${_CFG} ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})
endforeach()

# --- Collect all Slang shaders recursively ----------------------------------------
file(GLOB_RECURSE SLANG_ALL
        CONFIGURE_DEPENDS
        "${CMAKE_SOURCE_DIR}/Source/Game/Shaders/*.slang")

# Graphics = exclude files whose basename ends with _comp.slang or .comp.slang
set(SLANG_GRAPHICS "${SLANG_ALL}")
list(FILTER SLANG_GRAPHICS EXCLUDE REGEX "(_comp|\\.comp)\\.slang$")

# Compute = include only files whose basename ends with _comp.slang or .comp.slang
set(SLANG_COMP "${SLANG_ALL}")
list(FILTER SLANG_COMP INCLUDE REGEX "(_comp|\\.comp)\\.slang$")

set(COMPILED_SHADERS "")

# --- Graphics shaders: vertex+fragment --------------------------------------------
foreach(shader ${SLANG_GRAPHICS})
    get_filename_component(shader_name "${shader}" NAME_WE)
    set(output_spv "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/Shaders/${shader_name}.spv")

    add_custom_command(
            OUTPUT "${output_spv}"
            COMMAND "${CMAKE_COMMAND}" -E make_directory "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/Shaders"
            COMMAND "${CMAKE_SOURCE_DIR}/3rdParty/slang/bin/slangc.exe"
            -o "${output_spv}"
            -target spirv -profile glsl_460
            -entry vertexMain -entry fragmentMain
            -emit-spirv-directly -fvk-use-scalar-layout
            "${shader}"
            DEPENDS "${shader}" "${CMAKE_SOURCE_DIR}/3rdParty/slang/bin/slangc.exe"
            COMMENT "Compiling graphics shader ${shader_name}.slang -> SPIR-V"
            VERBATIM
    )
    list(APPEND COMPILED_SHADERS "${output_spv}")
endforeach()

# --- Compute shaders: compMain -----------------------------------------------------
foreach(cshader ${SLANG_COMP})
    get_filename_component(cshader_name "${cshader}" NAME_WE)
    set(cout_spv "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/Shaders/${cshader_name}.spv")

    add_custom_command(
            OUTPUT "${cout_spv}"
            COMMAND "${CMAKE_COMMAND}" -E make_directory "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/Shaders"
            COMMAND "${CMAKE_SOURCE_DIR}/3rdParty/slang/bin/slangc.exe"
            -o "${cout_spv}"
            -target spirv -profile glsl_460
            -entry compMain -emit-spirv-directly
            "${cshader}"
            DEPENDS "${cshader}" "${CMAKE_SOURCE_DIR}/3rdParty/slang/bin/slangc.exe"
            COMMENT "Compiling compute shader ${cshader_name}.slang -> SPIR-V"
            VERBATIM
    )
    list(APPEND COMPILED_SHADERS "${cout_spv}")
endforeach()

find_package(Vulkan REQUIRED)

# --- Shader build targets ----------------------------------------------------------
add_custom_target(Shaders ALL DEPENDS ${COMPILED_SHADERS})

# --- C/C++ sources ----------------------------------------------------------------
set(SRC_FILES
        Source/main.cpp

        ${TOOLS_DIR}/Arena.cpp
        ${CORE_DIR}/Platform/PlatformWindows.cpp

        ${CORE_DIR}/Renderer/Vulkan/VulkanInit.cpp
        ${CORE_DIR}/Renderer/Vulkan/VulkanSwapchain.cpp
        ${CORE_DIR}/Renderer/Vulkan/VulkanTexture.cpp
        ${CORE_DIR}/Renderer/Vulkan/VulkanCommands.cpp
        ${CORE_DIR}/Renderer/Vulkan/VulkanPipeline.cpp
        ${CORE_DIR}/Renderer/Vulkan/VulkanShader.cpp
        ${CORE_DIR}/Renderer/Vulkan/VulkanBuffer.cpp
        ${CORE_DIR}/Renderer/Vulkan/VulkanDescriptors.cpp
        ${CORE_DIR}/Renderer/Vulkan/VulkanRenderPass.cpp
        ${CORE_DIR}/Renderer/Vulkan/VulkanShaderBuffer.cpp
        ${CORE_DIR}/Renderer/Vulkan/VulkanQueryPool.cpp
        ${CORE_DIR}/Renderer/TextureManager.cpp
        ${CORE_DIR}/Renderer/MeshLoader.cpp
        ${ENGINE_DIR}/Camera.cpp
)

set(HDR_FILES
        ${CORE_DIR}/Renderer/RendererTypes.h
        ${CORE_DIR}/Renderer/MeshData.h
        #   ${CORE_DIR}/Math/Mat4x4.h

        ${CORE_DIR}/Renderer/Vulkan/VulkanSwapchain.h
        ${CORE_DIR}/Renderer/Vulkan/VulkanTexture.h
        ${CORE_DIR}/Renderer/Vulkan/VulkanConvert.h
        ${CORE_DIR}/Renderer/Vulkan/VulkanCommands.h
        ${CORE_DIR}/Renderer/Vulkan/VulkanPipeline.h
        ${CORE_DIR}/Renderer/Vulkan/VulkanShader.h
        ${CORE_DIR}/Renderer/Vulkan/VulkanBuffer.h
        ${CORE_DIR}/Renderer/Vulkan/VulkanCheck.h
        ${CORE_DIR}/Renderer/Vulkan/VulkanDescriptors.h
        ${CORE_DIR}/Renderer/Vulkan/VulkanRenderPass.h
        ${CORE_DIR}/Renderer/Vulkan/VulkanDebugUtils.h
        ${CORE_DIR}/Renderer/Vulkan/VulkanShaderBuffer.h
        ${CORE_DIR}/Renderer/Vulkan/scene.h

        ${CORE_DIR}/Renderer/TextureManager.h
        ${CORE_DIR}/Renderer/MeshLoader.h
        ${ENGINE_DIR}/Camera.h
        Source/Engine/EditorUi.cpp
        Source/Engine/EditorUi.h
        Source/Core/Renderer/MeshStats.h
        Source/Game/Application.cpp
        Source/Game/Application.h
        #   Source/Core/Renderer/ShaderCompiler.cpp
        #   Source/Core/Renderer/ShaderCompiler.h
        ${TOOLS_DIR}/MemStuff.cpp
        Source/Core/Input/InputSys.cpp
        Source/Core/Input/InputSys.h
        Source/Core/Renderer/Vulkan/VulkanQueryPool.h
        Source/Core/Renderer/Vulkan/VulkanMesh.cpp
        Source/Core/Renderer/Vulkan/VulkanMesh.h
#        ${TOOLS_DIR}/DeletionQueue.h
        ${TOOLS_DIR}/Timer.h
        Source/Core/Math/MikkWrapper.cpp
        Source/Core/Math/MikkWrapper.h
        Source/Core/Renderer/AABB.h
        Source/Core/Renderer/AABB.h
        Source/Engine/FPSCamera.cpp
        Source/Engine/FPSCamera.h
        Source/Core/Input/InputSysGameInput.cpp
        Source/Core/Input/InputSysGameInput.h
)

set(IMGUI_FILES
        ${3RDPARTY_DIR}/imgui/imgui.cpp
        ${3RDPARTY_DIR}/imgui/imgui_demo.cpp
        ${3RDPARTY_DIR}/imgui/imgui_draw.cpp
        ${3RDPARTY_DIR}/imgui/imgui_tables.cpp
        ${3RDPARTY_DIR}/imgui/imgui_widgets.cpp
        ${3RDPARTY_DIR}/imgui/backends/imgui_impl_vulkan.cpp
        ${3RDPARTY_DIR}/imgui/backends/imgui_impl_win32.cpp
        ${3RDPARTY_DIR}/imgui/imgui.h
        ${3RDPARTY_DIR}/imgui/imgui_internal.h
        ${3RDPARTY_DIR}/imgui/backends/imgui_impl_vulkan.h
        ${3RDPARTY_DIR}/imgui/backends/imgui_impl_win32.h
)

set(3RDPARTY_HEADERS
        ${3RDPARTY_DIR}/stb/stb_image.h
        ${3RDPARTY_DIR}/tinyobjloader/tiny_obj_loader.h
        ${3RDPARTY_DIR}/imoguizmo/imoguizmo.hpp
)

list(APPEND SRC_FILES ${IMGUI_FILES})
list(APPEND HDR_FILES ${3RDPARTY_HEADERS})

add_executable(OrgEngineV2 ${SRC_FILES} ${HDR_FILES})

# Make VS/IDE filters mirror the tree (nice for navigation)
source_group(TREE ${CMAKE_SOURCE_DIR} FILES ${SRC_FILES} ${HDR_FILES})

target_include_directories(OrgEngineV2 PRIVATE
        ${CORE_DIR}
        ${CORE_DIR}/Platform
        ${CORE_DIR}/Renderer
        ${CORE_DIR}/Renderer/Vulkan
        ${CORE_DIR}/Math
        ${ENGINE_DIR}
        ${GAME_DIR}
        ${3RDPARTY_DIR}/stb
        ${3RDPARTY_DIR}/tinyobjloader
        ${3RDPARTY_DIR}/imgui
        ${3RDPARTY_DIR}/imgui/backends
        ${3RDPARTY_DIR}/imoguizmo
        ${3RDPARTY_DIR}/fmt/include
        ${3RDPARTY_DIR}/volk
        ${3RDPARTY_DIR}/vma/include
        ${3RDPARTY_DIR}/fastgltf/include
        ${3RDPARTY_DIR}/meshoptimizer/src
        ${3RDPARTY_DIR}/mikktspace
        ${3RDPARTY_DIR}/glm
        ${3RDPARTY_DIR}/ufbx
        ${3RDPARTY_DIR}/tracy/public
        ${3RDPARTY_DIR}/slang/include
        ${3RDPARTY_DIR}/gameinput/include
        ${Vulkan_INCLUDE_DIRS}
)

add_subdirectory(3rdParty/fmt)
add_subdirectory(3rdParty/volk)
add_subdirectory(3rdParty/vma)
add_subdirectory(3rdParty/fastgltf)
add_subdirectory(3rdParty/meshoptimizer)
add_subdirectory(3rdParty/mikktspace)
add_subdirectory(3rdParty/glm)
add_subdirectory(3rdParty/ufbx)

option(TRACY_ENABLE "" OFF)
option(TRACY_ON_DEMAND "" OFF)
add_subdirectory(3rdParty/tracy)

# Link directories for external libs
target_link_directories(OrgEngineV2 PRIVATE 
        ${3RDPARTY_DIR}/slang/lib
        ${3RDPARTY_DIR}/gameinput/lib
)

target_precompile_headers(OrgEngineV2 PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Source/PrimTypes.h)
target_link_libraries(OrgEngineV2 PRIVATE 
        fmt::fmt 
        volk 
        GPUOpen::VulkanMemoryAllocator 
        slang 
        meshoptimizer 
        mikktspace 
        fastgltf 
        glm 
        GameInput.lib 
        ufbx
        Tracy::TracyClient
)

# Set output folder for the executable
set_target_properties(OrgEngineV2 PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
)

# --- Visual Studio & fast-iteration tuning ---------------------------------------
# Start this project in VS and use App/ as working folder (assets next to EXE)
set_directory_properties(PROPERTIES VS_STARTUP_PROJECT OrgEngineV2)
set_target_properties(OrgEngineV2 PROPERTIES
        VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}"
)

if (MSVC)
    # Multi-proc compile & Edit-and-Continue for Debug, fast PDB linking
    target_compile_options(OrgEngineV2 PRIVATE
            $<$<CONFIG:Debug>:/Zi /permissive->
            $<$<CONFIG:RelWithDebInfo>:/Zi>
            /MP /Zf
    )
    target_link_options(OrgEngineV2 PRIVATE
            $<$<CONFIG:Debug>:/DEBUG:FASTLINK /INCREMENTAL>
            $<$<CONFIG:RelWithDebInfo>:/DEBUG /INCREMENTAL>
            "/MAP:${CMAKE_BINARY_DIR}/App/OrgEngineV2.map" /MAPINFO:EXPORTS
    )
else()
    target_compile_options(OrgEngineV2 PRIVATE
            $<$<CONFIG:Debug>:-O0 -g>
            $<$<CONFIG:RelWithDebInfo>:-O2 -g>
    )
endif()

if (ORGENGINE_WARN_AS_ERROR)
    if (MSVC)
        target_compile_options(OrgEngineV2 PRIVATE /W4 /WX)
    else()
        target_compile_options(OrgEngineV2 PRIVATE -Wall -Wextra -Wpedantic -Werror)
    endif()
else()
    if (MSVC)
        target_compile_options(OrgEngineV2 PRIVATE /W4)
    else()
        target_compile_options(OrgEngineV2 PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endif()

# --- Final dependencies between app and shader compilation ------------------------
add_custom_target(CompileShaders ALL DEPENDS ${COMPILED_SHADERS})
add_dependencies(OrgEngineV2 CompileShaders)
