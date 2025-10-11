//
// Created by Orgest on 8/28/2025.
//

#include "MikkWrapper.h"
#include <cmath>

// User data passed to mikktspace
struct MikkUserData {
    Vertex*         vtx;
    const uint32_t* idx;
    uint32_t        indexCount; // triangle indices
};

// --------- mikktspace callbacks ----------
static int  mikkGetNumFaces(const SMikkTSpaceContext* ctx)
{
    auto* d = reinterpret_cast<MikkUserData*>(ctx->m_pUserData);
    return int(d->indexCount / 3);
}

static int  mikkGetNumVerticesOfFace(const SMikkTSpaceContext*, const int /*face*/)
{
    return 3;
}

static void mikkGetPosition(const SMikkTSpaceContext* ctx, float out[], const int face, const int vert)
{
    auto* d = reinterpret_cast<MikkUserData*>(ctx->m_pUserData);
    const Vertex& v = d->vtx[d->idx[face * 3 + vert]];
    out[0] = v.position.x; out[1] = v.position.y; out[2] = v.position.z;
}

static void mikkGetNormal(const SMikkTSpaceContext* ctx, float out[], const int face, const int vert)
{
    auto* d = reinterpret_cast<MikkUserData*>(ctx->m_pUserData);
    const Vertex& v = d->vtx[d->idx[face * 3 + vert]];
    out[0] = v.normal.x; out[1] = v.normal.y; out[2] = v.normal.z;
}

static void mikkGetTexCoord(const SMikkTSpaceContext* ctx, float out[], const int face, const int vert)
{
    auto* d = reinterpret_cast<MikkUserData*>(ctx->m_pUserData);
    const Vertex& v = d->vtx[d->idx[face * 3 + vert]];
    out[0] = v.uv.x; out[1] = v.uv.y;
}

static void mikkSetTSpaceBasic(const SMikkTSpaceContext* ctx,
                               const float tangent[3], const float sign,
                               const int face, const int vert)
{
    auto* d = reinterpret_cast<MikkUserData*>(ctx->m_pUserData);
    Vertex& v = d->vtx[d->idx[face * 3 + vert]];
    v.tangent.x = tangent[0];
    v.tangent.y = tangent[1];
    v.tangent.z = tangent[2];
    v.tangent.w = sign; // handedness (+1 / -1)
}

// --------- public entry ---------
void GenerateMikkTangents(Vertex* verts, uint32_t vertCount,
                          const uint32_t* indices, uint32_t indexCount)
{
    (void)vertCount; // not required by mikktspace, but we keep it for sanity checks
    // Preconditions: indexCount % 3 == 0, verts have valid normals & UVs
    SMikkTSpaceInterface iface{};
    iface.m_getNumFaces          = mikkGetNumFaces;
    iface.m_getNumVerticesOfFace = mikkGetNumVerticesOfFace;
    iface.m_getPosition          = mikkGetPosition;
    iface.m_getNormal            = mikkGetNormal;
    iface.m_getTexCoord          = mikkGetTexCoord;
    iface.m_setTSpaceBasic       = mikkSetTSpaceBasic;

    MikkUserData data{ verts, indices, indexCount };
    SMikkTSpaceContext ctx{ &iface, &data };

    // Generates tangent.xyz and handedness in .w
    genTangSpaceDefault(&ctx);
}
