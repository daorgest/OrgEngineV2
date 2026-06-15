//
// Created by Orgest on 8/28/2025.
//
#include <mikktspace.h>

#include "../../Engine/MeshData.h"

#include "Tools/Span.h"

namespace
{
    // User data passed to mikktspace
    struct MikkUserData
    {
        Span<Vertex> vertices;
        Span<const u32> indices;
    };
}

static i32 mikkGetNumFaces(const SMikkTSpaceContext* ctx)
{
    const auto* d = static_cast<MikkUserData*>(ctx->m_pUserData);
    return static_cast<i32>(d->indices.size() / 3);
}

static i32 mikkGetNumVerticesOfFace(const SMikkTSpaceContext* /*unused*/, const i32 /*face*/)
{
    return 3;
}

static void mikkGetPosition(const SMikkTSpaceContext* context, f32 posOut[], const i32 faceIdx, const i32 vertIdx)
{
    const auto* data = static_cast<MikkUserData*>(context->m_pUserData);
    const u32 index = data->indices[faceIdx * 3 + vertIdx];
    const auto& pos = data->vertices[index].position;
    posOut[0] = pos.x;
    posOut[1] = pos.y;
    posOut[2] = pos.z;
}

static void mikkGetNormal(const SMikkTSpaceContext* context, f32 normOut[], const i32 faceIdx, const i32 vertIdx)
{
    const auto* data = static_cast<MikkUserData*>(context->m_pUserData);
    const u32 index = data->indices[faceIdx * 3 + vertIdx];
    const auto& norm = data->vertices[index].normal;
    normOut[0] = norm.x;
    normOut[1] = norm.y;
    normOut[2] = norm.z;
}

static void mikkGetTexCoord(const SMikkTSpaceContext* context, f32 uvOut[], const i32 faceIdx, const i32 vertIdx)
{
    const auto* data = static_cast<MikkUserData*>(context->m_pUserData);
    const u32 index = data->indices[faceIdx * 3 + vertIdx];
    const auto& uv = data->vertices[index].uv;
    uvOut[0] = uv.x;
    uvOut[1] = uv.y;
}

static void mikkSetTSpaceBasic(const SMikkTSpaceContext* context, const f32 tangent[], const f32 sign,
                               const i32 faceIdx, const i32 vertIdx)
{
    auto* data = static_cast<MikkUserData*>(context->m_pUserData);
    const u32 index = data->indices[faceIdx * 3 + vertIdx];
    auto& v = data->vertices[index];
    v.tangent = glm::vec4(tangent[0], tangent[1], tangent[2], sign);
}

void GenerateMikkTangents(const Span<Vertex> vertices, const Span<const u32> indices)
{
    SMikkTSpaceInterface iface{};
    iface.m_getNumFaces = mikkGetNumFaces;
    iface.m_getNumVerticesOfFace = mikkGetNumVerticesOfFace;
    iface.m_getPosition = mikkGetPosition;
    iface.m_getNormal = mikkGetNormal;
    iface.m_getTexCoord = mikkGetTexCoord;
    iface.m_setTSpaceBasic = mikkSetTSpaceBasic;

    MikkUserData data{vertices, indices};
    const SMikkTSpaceContext ctx{&iface, &data};

    genTangSpaceDefault(&ctx);
}