[[vk::binding(0, 1)]] TextureCube    skyTexture;
[[vk::binding(1, 1)]] SamplerState   globalSampler;

struct Vertex
{
    float3 position;
    float3 normal;
    float4 tangent;
    float3 color;
    float2 uv;
};

struct SkyVertexOutput
{
    float4 position : SV_Position;
    float3 dir      : TEXCOORD0;
};

[shader("vertex")]
SkyVertexOutput vertexMain(uint vertexID : SV_VertexID, uniform float4x4 modelviewproj, uniform uint64_t vertexBufferAddress)
{
    SkyVertexOutput output;

    Ptr<Vertex> vertices = Ptr<Vertex>(vertexBufferAddress);
    float3 p = vertices[vertexID].position;

    output.position = mul(modelviewproj, float4(p, 1.0));
    output.position.z = output.position.w; // force depth = 1.0 after divide
    output.dir      = p;
    return output;
}

[shader("fragment")]
float4 fragmentMain(SkyVertexOutput input) : SV_Target
{
    return skyTexture.Sample(globalSampler, normalize(input.dir));
}