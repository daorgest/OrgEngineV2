cmake_minimum_required(VERSION 3.31)
project(OrgEngineV2)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CORE_DIR     ${CMAKE_CURRENT_SOURCE_DIR}/Source/Core)
set(ENGINE_DIR   ${CMAKE_CURRENT_SOURCE_DIR}/Source/Engine)
set(GAME_DIR     ${CMAKE_CURRENT_SOURCE_DIR}/Source/Game)
set(3RDPARTY_DIR ${CMAKE_SOURCE_DIR}/3rdParty)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)
add_compile_definitions(UNICODE VK_NO_PROTOTYPES VOLK_STATIC_DEFINES VK_USE_PLATFORM_WIN32_KHR NOMINMAX EDITORUI
        IMGUI_IMPL_VULKAN_USE_VOLK VULKAN_DEBUG_MODE)

# Git Version Hash
execute_process(
        COMMAND git rev-parse --short HEAD
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        OUTPUT_VARIABLE GIT_HASH
        OUTPUT_STRIP_TRAILING_WHITESPACE
)

add_compile_definitions(ENGINE_COMMIT_HASH="${GIT_HASH}")

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/App)

# Copy shaders to App/Shaders
file(GLOB SLANG_SHADER_FILES "${CMAKE_SOURCE_DIR}/Source/Game/Shaders/*.slang")

foreach(shader ${SLANG_SHADER_FILES})
    get_filename_component(shader_name ${shader} NAME_WE)
    set(output_spv "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/Shaders/${shader_name}.spv")

    # Detect if it’s a compute shader by name (e.g., ends with _comp)
    if("${shader_name}" MATCHES ".*comp.*")
        add_custom_command(
                OUTPUT ${output_spv}
                COMMAND ${CMAKE_SOURCE_DIR}/3rdParty/slang/bin/slangc.exe
                -o ${output_spv}
                -target spirv -profile glsl_460  -entry compMain -emit-spirv-directly
                ${shader}
                DEPENDS ${shader}
                COMMENT "Compiling compute shader ${shader_name}.slang to SPIR-V"
        )
    else()
        add_custom_command(
                OUTPUT ${output_spv}
                COMMAND ${CMAKE_SOURCE_DIR}/3rdParty/slang/bin/slangc.exe
                -o ${output_spv}
                -target spirv -profile glsl_460
                -entry vertexMain -entry fragmentMain -emit-spirv-directly -fvk-use-scalar-layout
                ${shader}
                DEPENDS ${shader}
                COMMENT "Compiling graphics shader ${shader_name}.slang to SPIR-V"
        )
    endif()

    list(APPEND COMPILED_SHADERS ${output_spv})
endforeach()

add_custom_target(Shaders ALL DEPENDS ${COMPILED_SHADERS})


set(SRC_FILES
        Source/main.cpp

        ${CORE_DIR}/Arena.cpp
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
        Source/Core/Renderer/Vulkan/VulkanQueryPool.cpp
        ${CORE_DIR}/Renderer/TextureManager.cpp
        ${CORE_DIR}/Renderer/MeshLoader.cpp
        ${ENGINE_DIR}/Camera.cpp
)

set(HDR_FILES
        ${CORE_DIR}/PrimTypes.h
        ${CORE_DIR}/Renderer/RendererTypes.h
        ${CORE_DIR}/Renderer/MeshData.h
        ${CORE_DIR}/Math/Mat4x4.h

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
#        Source/Core/Renderer/ShaderCompiler.cpp
#        Source/Core/Renderer/ShaderCompiler.h
        Source/Core/MemStuff.cpp
        Source/Core/Input/InputSys.cpp
        Source/Core/Input/InputSys.h
        Source/Core/Renderer/Vulkan/VulkanQueryPool.h
        Source/Core/Renderer/Vulkan/VulkanMesh.cpp
        Source/Core/Renderer/Vulkan/VulkanMesh.h
        Source/Core/DeletionQueue.h
        Source/Core/Timer.h
)

list(APPEND SRC_FILES
        3rdParty/imgui/imgui.cpp
        3rdParty/imgui/imgui_demo.cpp
        3rdParty/imgui/imgui_draw.cpp
        3rdParty/imgui/imgui_tables.cpp
        3rdParty/imgui/imgui_widgets.cpp
        3rdParty/imgui/backends/imgui_impl_vulkan.cpp
        3rdParty/imgui/backends/imgui_impl_win32.cpp
)

add_executable(OrgEngineV2 ${SRC_FILES} ${HDR_FILES} ${3RDPARTY_FILES})

target_include_directories(OrgEngineV2 PRIVATE
        ${CORE_DIR}
        ${CORE_DIR}/Platform
        ${CORE_DIR}/Renderer
        ${CORE_DIR}/Renderer/Vulkan
        ${CORE_DIR}/Math
        ${ENGINE_DIR}
        ${GAME_DIR}
        3rdParty/stb
        3rdParty/tinyobjloader
        3rdParty/imgui
        3rdParty/imgui/backends
        3rdParty/imoguizmo
)

add_subdirectory(3rdParty/fmt)
add_subdirectory(3rdParty/volk)
add_subdirectory(3rdParty/vma)

option (TRACY_ENABLE "" ON)
option (TRACY_ON_DEMAND "" ON)
add_subdirectory(3rdParty/tracy)
target_link_libraries(OrgEngineV2 PUBLIC Tracy::TracyClient)

target_include_directories(OrgEngineV2 PRIVATE ${3RDPARTY_DIR}/slang/include)
target_link_directories(OrgEngineV2 PRIVATE ${3RDPARTY_DIR}/slang/lib)

target_precompile_headers(OrgEngineV2 PRIVATE ${CORE_DIR}/PrimTypes.h)
target_link_libraries(OrgEngineV2 PRIVATE fmt::fmt volk GPUOpen::VulkanMemoryAllocator slang)

# Set output folder for the executable
set_target_properties(OrgEngineV2 PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
)

if (MSVC)
    set(MAP_FILE "${CMAKE_BINARY_DIR}/App/OrgEngineV2.map")
    target_link_options(OrgEngineV2 PRIVATE "/MAP:${MAP_FILE}" /MAPINFO:EXPORTS)
endif()

# Add a dummy target to run them all
add_custom_target(CompileShaders ALL DEPENDS ${COMPILED_SHADERS})
add_dependencies(OrgEngineV2 CompileShaders)