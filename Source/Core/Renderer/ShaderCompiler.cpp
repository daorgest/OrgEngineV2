//
// Created by Orgest on 7/16/2025.
//

#include "ShaderCompiler.h"

ShaderCompiler::ShaderCompiler()
{
	SlangGlobalSessionDesc desc {.enableGLSL = true};
	slang::createGlobalSession(&desc, globalSession.writeRef());

	targets.push_back({
		.format = SLANG_SPIRV,
		.profile = globalSession->findProfile("spirv_1_6+vulkan_1_4"),
		.forceGLSLScalarBufferLayout = true
	});

	compilerOptions.clear();
	SetDefaultCompilerOptions();

}

void ShaderCompiler::SetDefaultCompilerOptions()
{
	// Emit SPIR-V directly instead of going through GLSL
	compilerOptions.push_back({slang::CompilerOptionName::EmitSpirvDirectly, {slang::CompilerOptionValueKind::Int, 1}});

	// // Language: GLSL (useful if parsing GLSL instead of HLSL)
	// compilerOptions.push_back({slang::CompilerOptionName::Language, {slang::CompilerOptionValueKind::String, 0, 0, "glsl"}});

	// Matrix layout: Column-major (standard in Vulkan)
	compilerOptions.push_back({slang::CompilerOptionName::MatrixLayoutColumn, {slang::CompilerOptionValueKind::Int, 1}});

	// Enforce GLSL “scalar block layout” (good for std430-like layouts)
	compilerOptions.push_back({slang::CompilerOptionName::GLSLForceScalarLayout, {slang::CompilerOptionValueKind::Int, 1}});

	// Emit SPIR-V reflection data
	compilerOptions.push_back({slang::CompilerOptionName::VulkanEmitReflection, {slang::CompilerOptionValueKind::Int, 1}});

	// Debug info: include minimal debug info in SPIR-V
	compilerOptions.push_back({slang::CompilerOptionName::DebugInformation, {slang::CompilerOptionValueKind::Int, 1}});

	// Optional: enable warnings as errors
	compilerOptions.push_back({slang::CompilerOptionName::WarningsAsErrors, {slang::CompilerOptionValueKind::String, 0, 0, "all"}});

}


