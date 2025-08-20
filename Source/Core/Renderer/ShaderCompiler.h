//
// Created by Orgest on 7/16/2025.
//

#pragma once
#include <slang.h>
#include <slang-com-ptr.h>

#include "OrgString.h"
#include "Vector.h"

struct ShaderCompiler
{
	ShaderCompiler();
	~ShaderCompiler() = default;

	void SetDefaultCompilerOptions();

private:
	Slang::ComPtr<slang::IGlobalSession> globalSession;
	Vector<slang::TargetDesc> targets;
	Vector<slang::CompilerOptionEntry> compilerOptions;
	Vector<OrgString> searchPaths;
	Slang::ComPtr<slang::ISession> session;
	Slang::ComPtr<slang::IModule> module;
	Slang::ComPtr<slang::IComponentType> componentType;
	Slang::ComPtr<ISlangBlob> blob;
	Vector<slang::PreprocessorMacroDesc> macros;

};
