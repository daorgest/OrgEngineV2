//
// Created by Orgest on 11/19/2025.
//

#pragma once
#include <filesystem>

#include "../PrimTypes.h"
#include "Tools/Vector.h"
#include <slang/slang.h>
#include <slang/slang-com-ptr.h>

#include "RendererTypes.h"

namespace Renderer
{
    struct CompileResult
    {
        Vector<u32> code;
        PipelineLayoutDesc layoutDesc;
    };

    struct ShaderCompiler
    {
        void Init();
        CompileResult CompileShader(const std::filesystem::path& filePath) const;
        static PipelineLayoutDesc ReflectLayout(slang::IComponentType* program);
        static void ReflectPushConstants(slang::IComponentType* program, PipelineLayoutDesc& outLayout);

        Slang::ComPtr<slang::IGlobalSession> globalSession;
    };
}