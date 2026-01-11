// --- RESOURCES ---
[[vk::binding(0, 0)]] SamplerCube skyTexture;

// --- PUSH CONSTANTS ---
struct PushConstants
{
    float4x4 viewProjection;
};
[[vk::push_constant]] ConstantBuffer<PushConstants> pc;


struct SkyVertexOutput
{
    float4 position : SV_Position;
    float3 dir      : TEXCOORD0;
};

// --- STATIC SKYBOX GEOMETRY (INDEXED) ---
// 8 unique vertices for the cube corners
static const float3 g_skyboxVertices[8] = {
    float3(-0.5f, -0.5f,  0.5f), // 0: front-bottom-left
    float3( 0.5f, -0.5f,  0.5f), // 1: front-bottom-right
    float3( 0.5f,  0.5f,  0.5f), // 2: front-top-right
    float3(-0.5f,  0.5f,  0.5f), // 3: front-top-left
    float3( 0.5f, -0.5f, -0.5f), // 4: back-bottom-right
    float3(-0.5f, -0.5f, -0.5f), // 5: back-bottom-left
    float3(-0.5f,  0.5f, -0.5f), // 6: back-top-left
    float3( 0.5f,  0.5f, -0.5f)  // 7: back-top-right
};

// 36 indices to draw the 12 triangles of the cube
static const uint g_skyboxIndices[36] = {
    0, 1, 2, 2, 3, 0, // Front
    4, 5, 6, 6, 7, 4, // Back
    1, 4, 7, 7, 2, 1, // Right
    5, 0, 3, 3, 6, 5, // Left
    3, 2, 7, 7, 6, 3, // Top
    5, 4, 1, 1, 0, 5  // Bottom
};


[shader("vertex")]
SkyVertexOutput vertexMain(uint vertexID : SV_VertexID)
{
    SkyVertexOutput output;

    uint index = g_skyboxIndices[vertexID];
    float3 localPos = g_skyboxVertices[index];

    float4 pos = mul(pc.viewProjection, float4(localPos, 1.0));

    // Standard depth: Set z=w for far plane (after perspective divide: z/w = 1.0)
    // When drawn LAST with depth test ON, skybox only renders where depth buffer == 1.0 (no geometry)
    output.position = float4(pos.xy, 0.0, pos.w); // z=w, after divide: 1.0 (far plane)


    output.dir = localPos;
    return output;
}


[shader("fragment")]
float4 fragmentMain(SkyVertexOutput input) : SV_Target
{
    return skyTexture.Sample(input.dir);
}