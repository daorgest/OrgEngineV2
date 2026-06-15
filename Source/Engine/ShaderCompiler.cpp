//
// Created by Orgest on 4/7/2026.
//

#include "ShaderCompiler.h"

#include "Tools/Array.h"
#include "Tools/Logger.h"

using namespace Renderer;

[[nodiscard]] constexpr SlangStage ToSlangStage(ShaderStage stage)
{
    switch (stage)
    {
    case ShaderStage::Vertex: return SLANG_STAGE_VERTEX;
    case ShaderStage::TessControl: return SLANG_STAGE_HULL;
    case ShaderStage::TessEvaluation: return SLANG_STAGE_DOMAIN;
    case ShaderStage::Geometry: return SLANG_STAGE_GEOMETRY;
    case ShaderStage::Fragment: return SLANG_STAGE_FRAGMENT;
    case ShaderStage::Compute: return SLANG_STAGE_COMPUTE;
    case ShaderStage::Mesh: return SLANG_STAGE_MESH;
    case ShaderStage::RayGen: return SLANG_STAGE_RAY_GENERATION;
    case ShaderStage::AnyHit: return SLANG_STAGE_ANY_HIT;
    case ShaderStage::ClosestHit: return SLANG_STAGE_CLOSEST_HIT;
    case ShaderStage::Miss: return SLANG_STAGE_MISS;
    case ShaderStage::Intersection: return SLANG_STAGE_INTERSECTION;
    case ShaderStage::Callable: return SLANG_STAGE_CALLABLE;
    default: return SLANG_STAGE_NONE;
    }
}

[[nodiscard]] constexpr ShaderStage FromSlangStage(SlangStage stage)
{
    switch (stage)
    {
    case SLANG_STAGE_VERTEX: return ShaderStage::Vertex;
    case SLANG_STAGE_HULL: return ShaderStage::TessControl;
    case SLANG_STAGE_DOMAIN: return ShaderStage::TessEvaluation;
    case SLANG_STAGE_GEOMETRY: return ShaderStage::Geometry;
    case SLANG_STAGE_FRAGMENT: return ShaderStage::Fragment;
    case SLANG_STAGE_COMPUTE: return ShaderStage::Compute;
    case SLANG_STAGE_MESH: return ShaderStage::Mesh;
    case SLANG_STAGE_RAY_GENERATION: return ShaderStage::RayGen;
    case SLANG_STAGE_ANY_HIT: return ShaderStage::AnyHit;
    case SLANG_STAGE_CLOSEST_HIT: return ShaderStage::ClosestHit;
    case SLANG_STAGE_MISS: return ShaderStage::Miss;
    case SLANG_STAGE_INTERSECTION: return ShaderStage::Intersection;
    case SLANG_STAGE_CALLABLE: return ShaderStage::Callable;
    default: return ShaderStage::None;
    }
}

[[nodiscard]] constexpr SlangResourceShape ToSlangResourceShape(DescriptorType type)
{
    switch (type)
    {
    case DescriptorType::SampledImage: return SLANG_TEXTURE_2D;
    case DescriptorType::CombinedImageSampler: return (SlangResourceShape)(SLANG_TEXTURE_2D |
            SLANG_TEXTURE_COMBINED_FLAG);
    case DescriptorType::StorageImage: return SLANG_TEXTURE_2D;
    case DescriptorType::StorageBuffer: return SLANG_STRUCTURED_BUFFER;
    case DescriptorType::AccelerationStructure: return SLANG_ACCELERATION_STRUCTURE;
    case DescriptorType::InputAttachment: return SLANG_TEXTURE_SUBPASS;
    default: return SLANG_RESOURCE_NONE;
    }
}

void ShaderCompiler::Init()
{
    slang::createGlobalSession(globalSession.writeRef());
}

void ShaderCompiler::Destroy()
{
    if (globalSession)
    {
        globalSession = nullptr;
    }
}

CompileResult ShaderCompiler::CompileShader(const std::filesystem::path& filePath) const
{
    CompileResult result;


    Array compilerOptionEntries = {
        slang::CompilerOptionEntry{
            slang::CompilerOptionName::EmitSpirvDirectly, {slang::CompilerOptionValueKind::Int, 1, 0, nullptr, nullptr}
        },
        slang::CompilerOptionEntry{
            slang::CompilerOptionName::GLSLForceScalarLayout,
            {slang::CompilerOptionValueKind::Int, 1, 0, nullptr, nullptr}
        },
        slang::CompilerOptionEntry{
            slang::CompilerOptionName::MatrixLayoutColumn, {slang::CompilerOptionValueKind::Int, 1, 0, nullptr, nullptr}
        },
        slang::CompilerOptionEntry{
            slang::CompilerOptionName::Optimization, {slang::CompilerOptionValueKind::Int, 3, 0, nullptr, nullptr}
        },
        slang::CompilerOptionEntry{
            slang::CompilerOptionName::VulkanUseEntryPointName,
            {slang::CompilerOptionValueKind::Int, 1, 0, nullptr, nullptr}
        }
    };

    const std::string shadersDir = std::string(ENGINE_SOURCE_DIR) + "/Shaders/";
    const std::string gameShadersDir = std::string(ENGINE_SOURCE_DIR) + "/Source/Game/Shaders/";

    Array searchPaths = {
        shadersDir.c_str(),
        gameShadersDir.c_str()
    };

    slang::TargetDesc targetDesc = {
        .format = SLANG_SPIRV,
        .profile = globalSession->findProfile("sm_6_6"),
        .compilerOptionEntries = compilerOptionEntries.data(),
        .compilerOptionEntryCount = (u32)compilerOptionEntries.size()
    };

    slang::SessionDesc sessionDesc = {
        .targets = &targetDesc,
        .targetCount = 1,
        .searchPaths = searchPaths.data(),
        .searchPathCount = searchPaths.size(),
        .compilerOptionEntries = compilerOptionEntries.data(),
        .compilerOptionEntryCount = (u32)compilerOptionEntries.size()
    };

    Slang::ComPtr<slang::ISession> localSession;
    globalSession->createSession(sessionDesc, localSession.writeRef());

    const std::string moduleName = filePath.stem().string();
    Slang::ComPtr<slang::IBlob> diagnostics;
    Slang::ComPtr<slang::IModule> module;
    module.attach(localSession->loadModule(moduleName.c_str(), diagnostics.writeRef()));

    if (!module)
    {
        if (diagnostics && diagnostics->getBufferSize() > 0)
        {
            std::string errorStr(static_cast<const char*>(diagnostics->getBufferPointer()), diagnostics->getBufferSize());
            LOG(Warning, "[Slang Syntax Error]\n{}", errorStr.c_str());
        }
        return result;
    }

    Vector<slang::IComponentType*> components;
    components.push_back(module);

    const bool isCompute = filePath.string().find("_comp") != std::string::npos ||
        filePath.extension() == ".comp";

    Slang::ComPtr<slang::IEntryPoint> entry1;
    if (isCompute)
    {
        module->findEntryPointByName("compMain", entry1.writeRef());
        if (entry1) components.push_back(entry1);
    }
    else
    {
        Slang::ComPtr<slang::IEntryPoint> entry2;
        module->findEntryPointByName("vertexMain", entry1.writeRef());
        module->findEntryPointByName("fragmentMain", entry2.writeRef());
        if (entry1) components.push_back(entry1);
        if (entry2) components.push_back(entry2);
    }

    Slang::ComPtr<slang::IComponentType> linkedProgram;
    Slang::ComPtr<slang::IBlob> linkDiagnostics;
    localSession->createCompositeComponentType(
        components.data(),
        components.size(),
        linkedProgram.writeRef(),
        linkDiagnostics.writeRef()
    );
    if (!linkedProgram) return result;

    Slang::ComPtr<slang::IBlob> spirvBlob;
    linkedProgram->getTargetCode(0, spirvBlob.writeRef());

    if (!linkedProgram)
    {
        if (linkDiagnostics && linkDiagnostics->getBufferSize() > 0)
        {
            std::string errorStr(static_cast<const char*>(linkDiagnostics->getBufferPointer()), linkDiagnostics->getBufferSize());
            LOG(Warning, "[Slang Linker Error]\n{}", errorStr.c_str());
        }
        return result;
    }

    if (spirvBlob)
    {
        const auto codePtr = static_cast<const u32*>(spirvBlob->getBufferPointer());
        const size_t codeSize = spirvBlob->getBufferSize() / sizeof(u32);
        result.code.assign({codePtr, codeSize});

        result.layoutDesc = ReflectLayout(linkedProgram);
        ReflectPushConstants(linkedProgram, result.layoutDesc);
    }

    return result;
}

static inline DescriptorType mapToDescriptorType(slang::TypeReflection* type, slang::TypeLayoutReflection*)
{
    const auto kind = type->getKind();

    if (kind == slang::TypeReflection::Kind::ConstantBuffer ||
        kind == slang::TypeReflection::Kind::ParameterBlock)
        return DescriptorType::UniformBuffer;

    if (kind == slang::TypeReflection::Kind::Resource)
    {
        const SlangResourceShape shape = type->getResourceShape();
        const SlangResourceAccess access = type->getResourceAccess();


        // Buffer (StructuredBuffer / ByteAddressBuffer)
        const u32 baseShape = shape & SLANG_RESOURCE_BASE_SHAPE_MASK;
        if (baseShape == SLANG_STRUCTURED_BUFFER || baseShape == SLANG_BYTE_ADDRESS_BUFFER)
        {
            return DescriptorType::StorageBuffer;
        }

        // Combined Sampler (Sampler2D) vs Sampled Image (Texture2D)
        if (shape & SLANG_TEXTURE_COMBINED_FLAG)
        {
            return DescriptorType::CombinedImageSampler;
        }

        // If it's a texture but has Read/Write access, it's a Storage Image
        if (access == SLANG_RESOURCE_ACCESS_READ_WRITE)
        {
            return DescriptorType::StorageImage;
        }

        return DescriptorType::SampledImage;
    }

    if (kind == slang::TypeReflection::Kind::SamplerState)
    {
        return DescriptorType::Sampler;
    }

    return DescriptorType::UniformBuffer; // Default fallback
}

static Binding ReflectBinding(slang::VariableLayoutReflection* varLayout)
{
    Binding b;

    b.binding = (u32)varLayout->getOffset(SLANG_PARAMETER_CATEGORY_DESCRIPTOR_TABLE_SLOT);
    u32 setIndex = (u32)varLayout->getOffset(SLANG_PARAMETER_CATEGORY_REGISTER_SPACE);

    slang::TypeLayoutReflection* typeLayout = varLayout->getTypeLayout();
    slang::TypeReflection* type = typeLayout->getType();

    if (type->getKind() == slang::TypeReflection::Kind::Array)
    {
        size_t elementCount = type->getElementCount();
        if (elementCount == (size_t)-1 || elementCount == 0)
        {
            LOG(Info, "[Reflection] Bindless Array {} detected (Set {}, Binding {})", varLayout->getName(), setIndex,
                b.binding);
            b.isBindless = true;
            b.count = 1000;
        }
        else
        {
            b.isBindless = false;
            b.count = (u32)elementCount;
        }

        typeLayout = typeLayout->getElementTypeLayout();
        type = typeLayout->getType();
    }
    else
    {
        b.isBindless = false;
        b.count = 1;
    }

    b.type = mapToDescriptorType(type, typeLayout);

    if (b.type == DescriptorType::UniformBuffer || b.type == DescriptorType::StorageBuffer)
    {
        // For ConstantBuffer<T> or StructuredBuffer<T>, we want the size of T
        if (type->getKind() == slang::TypeReflection::Kind::ConstantBuffer ||
            type->getKind() == slang::TypeReflection::Kind::Resource)
        {
            auto* elementLayout = typeLayout->getElementTypeLayout();
            b.size = elementLayout ? elementLayout->getSize() : typeLayout->getSize();
        }
        else
        {
            b.size = typeLayout->getSize();
        }
    }
    else
    {
        b.size = 0;
    }


    b.stageFlags = ShaderStage::All; // global

    return b;
}

PipelineLayoutDesc ShaderCompiler::ReflectLayout(slang::IComponentType* program)
{
    thread_local Vector<DescriptorSetLayoutDesc> sets;
    sets.clear();

    PipelineLayoutDesc layoutDesc;
    auto* reflection = program->getLayout();

    // 1. Efficiency: Determine Stage Flags ONCE instead of every loop iteration
    ShaderStageFlags stageFlags = ShaderStage::None;
    const size_t entryCount = reflection->getEntryPointCount();
    for (u32 i = 0; i < entryCount; i++)
    {
        stageFlags |= FromSlangStage(reflection->getEntryPointByIndex(i)->getStage());
    }

    int setIndexToVecIdx[8];
    for (int i = 0; i < 8; ++i) setIndexToVecIdx[i] = -1;

    for (size_t i = 0; i < reflection->getParameterCount(); i++)
    {
        auto* var = reflection->getParameterByIndex(i);

        if ((u32)var->getCategory() == (u32)SLANG_PARAMETER_CATEGORY_PUSH_CONSTANT_BUFFER)
        {
            continue;
        }

        Binding b = ReflectBinding(var);
        b.stageFlags = stageFlags;

        const u32 setIndex = var->getBindingSpace();
        b.binding = var->getBindingIndex();

        if (setIndex >= 8)
        {
            LOG(Error, "[Reflection] Set index {} out of bounds!", setIndex);
            continue;
        }

        // Initialize a new DescriptorSetLayoutDesc in the scratchpad if this is a new set.
        if (setIndexToVecIdx[setIndex] == -1)
        {
            setIndexToVecIdx[setIndex] = (int)sets.size();
            sets.push_back({.setIndex = setIndex});
        }

        sets[setIndexToVecIdx[setIndex]].bindings.push_back(b);
    }

    // Explicitly copy the data from the scratchpad into the struct
    layoutDesc.setLayouts.assign(Span<const DescriptorSetLayoutDesc>(sets.data(), sets.size()));

    for (const auto& setLayout : layoutDesc.setLayouts)
    {
        LOG(Info, "[Reflection] Finalized Set {} with {} bindings",
            setLayout.setIndex, static_cast<u32>(setLayout.bindings.size()));
    }

    return layoutDesc;
}

void ShaderCompiler::ReflectPushConstants(slang::IComponentType* program, PipelineLayoutDesc& outLayout)
{
    auto* reflection = program->getLayout();
    size_t parameterCount = reflection->getParameterCount();

    ShaderStageFlags stageFlags = ShaderStage::None;
    const size_t entryCount = reflection->getEntryPointCount();
    for (size_t i = 0; i < entryCount; i++)
    {
        stageFlags |= FromSlangStage(reflection->getEntryPointByIndex(i)->getStage());
    }

    for (u32 i = 0; i < parameterCount; i++)
    {
        auto* var = reflection->getParameterByIndex(i);

        if ((u32)var->getCategory() == static_cast<u32>(SLANG_PARAMETER_CATEGORY_PUSH_CONSTANT_BUFFER))
        {
            auto* typeLayout = var->getTypeLayout();
            if (typeLayout->getKind() == slang::TypeReflection::Kind::ConstantBuffer ||
                typeLayout->getKind() == slang::TypeReflection::Kind::ParameterBlock)
            {
                typeLayout = typeLayout->getElementTypeLayout();
            }

            PushConstantDesc pc = {};
            pc.stages = stageFlags;
            pc.size = (u32)typeLayout->getSize();

            bool alreadyExists = false;
            for (const auto& existingPc : outLayout.pushConstants)
            {
                if (existingPc.size == pc.size)
                {
                    // Simple check, or compare names if needed
                    alreadyExists = true;
                    break;
                }
            }

            if (!alreadyExists)
            {
                outLayout.pushConstants.push_back(pc);
                LOG(Info, "[Reflection] Found Push Constant: {} | Size: {} bytes | Stages: {}",
                    var->getName(), pc.size, (u32)pc.stages);
            }
        }
    }
}
