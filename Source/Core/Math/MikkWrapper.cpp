//
// Created by Orgest on 8/28/2025.
//
#include <mikktspace.h>

#include "../../Engine/MeshData.h"

// User data passed to mikktspace
struct MikkUserData
{
    std::span<Vertex> vertices;
    std::span<const uint32_t> indices;
};

static int mikkGetNumFaces(const SMikkTSpaceContext* ctx)
{
    auto* d = static_cast<MikkUserData*>(ctx->m_pUserData);
    return static_cast<int>(d->indices.size() / 3);
}

static int mikkGetNumVerticesOfFace(const SMikkTSpaceContext* /*unused*/, const int /*face*/)
{
    return 3;
}

static void mikkGetPosition(const SMikkTSpaceContext* context, float posOut[], const int faceIdx, const int vertIdx)
{
    const auto* data = static_cast<MikkUserData*>(context->m_pUserData);
    const uint32_t index = data->indices[faceIdx * 3 + vertIdx];
    const auto& pos = data->vertices[index].position;
    posOut[0] = pos.x;
    posOut[1] = pos.y;
    posOut[2] = pos.z;
}

static void mikkGetNormal(const SMikkTSpaceContext* context, float normOut[], const int faceIdx, const int vertIdx)
{
    const auto* data = static_cast<MikkUserData*>(context->m_pUserData);
    const uint32_t index = data->indices[faceIdx * 3 + vertIdx];
    const auto& norm = data->vertices[index].normal;
    normOut[0] = norm.x;
    normOut[1] = norm.y;
    normOut[2] = norm.z;
}

static void mikkGetTexCoord(const SMikkTSpaceContext* context, float uvOut[], const int faceIdx, const int vertIdx)
{
    const auto* data = static_cast<MikkUserData*>(context->m_pUserData);
    const uint32_t index = data->indices[faceIdx * 3 + vertIdx];
    const auto& uv = data->vertices[index].uv;
    uvOut[0] = uv.x;
    uvOut[1] = uv.y;
}

static void mikkSetTSpaceBasic(const SMikkTSpaceContext* context, const float tangent[], const float sign,
                               const int faceIdx, const int vertIdx)
{
    auto* data = static_cast<MikkUserData*>(context->m_pUserData);
    const uint32_t index = data->indices[faceIdx * 3 + vertIdx];
    auto& v = data->vertices[index];
    v.tangent = glm::vec4(tangent[0], tangent[1], tangent[2], sign);
}

void GenerateMikkTangents(const std::span<Vertex> vertices, const std::span<const uint32_t> indices)
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