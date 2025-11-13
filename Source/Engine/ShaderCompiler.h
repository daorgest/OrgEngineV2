//
// Created by Orgest on 11/19/2025.
//

#pragma once
#include <filesystem>
#include <slang.h>

#include "RendererTypes.h"
#include "slang-com-ptr.h"

struct CompiledShader
{
	Slang::ComPtr<slang::IBlob> binary;
	ShaderStage stage = ShaderStage::None;
	std::string entryPoint;
	Vector<Binding> bindings;
	slang::IComponentType* reflection = nullptr;

	[[nodiscard]] bool IsValid() const
	{
		return binary && binary->getBufferSize() > 0 && stage != ShaderStage::None;
	}
};

class ShaderCompiler
{
public:
	ShaderCompiler();
	~ShaderCompiler();
	Result<void> Init();
	Result<CompiledShader> CompileShader(const std::filesystem::path& path) const;
private:
	slang::IGlobalSession* slangGlobalSession = nullptr;
	slang::ISession* slangSession = nullptr;
};