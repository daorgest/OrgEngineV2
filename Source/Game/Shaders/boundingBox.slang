struct VSOut
{
    float4 positionCS : SV_Position;
    float4 color      : COLOR0;
};

[[vk::binding(6, 0)]]
cbuffer SceneUBO
{
    float4x4 view;
    float4x4 proj;
};

struct BBoxPush
{
    float4x4 model;
    float3   aabbMin;   float depthBias;
    float3   aabbMax;   uint  flags;
    float4   color;
};

[[vk::binding(0, 1)]]
StructuredBuffer<BBoxPush> instances;

static const uint2 kEdgeCorners[12] = {
    uint2(0,1), uint2(1,3), uint2(3,2), uint2(2,0),
    uint2(4,5), uint2(5,7), uint2(7,6), uint2(6,4),
    uint2(0,4), uint2(1,5), uint2(2,6), uint2(3,7),
};

float3 cornerFromIndex(uint idx, float3 bmin, float3 bmax)
{
    float3 p;
    p.x = ((idx & 1u) != 0u) ? bmax.x : bmin.x;
    p.y = ((idx & 2u) != 0u) ? bmax.y : bmin.y;
    p.z = ((idx & 4u) != 0u) ? bmax.z : bmin.z;
    return p;
}

float4 applyDepthBiasCS(float4 posCS, float biasAmount)
{
    if (biasAmount == 0.0) return posCS;
    posCS.z -= biasAmount * posCS.w;
    return posCS;
}

[shader("vertex")]
VSOut vertexMain(uint vertexID : SV_VertexID, uint instanceID : SV_InstanceID)
{
    VSOut o;

    BBoxPush pc = instances[instanceID];

    // --- Determine line edge ---
    const uint edge   = (vertexID >> 1);
    const bool endSel = (vertexID & 1u) != 0u;
    const uint2 ec    = kEdgeCorners[edge];
    const uint  cIdx  = endSel ? ec.y : ec.x;

    float3 pLocal = cornerFromIndex(cIdx, pc.aabbMin, pc.aabbMax);
    float4 pWorld = mul(pc.model, float4(pLocal, 1.0));
    float4 pView  = mul(view, pWorld);
    float4 pClip  = mul(proj, pView);
    pClip = applyDepthBiasCS(pClip, pc.depthBias);

    o.positionCS = pClip;
    o.color      = pc.color;
    return o;
}

[shader("fragment")]
float4 fragmentMain(VSOut input) : SV_Target
{
    return input.color;
}
