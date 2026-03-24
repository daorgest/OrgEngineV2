struct PushConstants
{
    float4x4 model;
    float4x4 lightSpaceMatrix;
};

[[vk::push_constant]] PushConstants pc;

struct VertexInput
{
    float3 position : POSITION;
};

struct Coords
{
    float4 clipPos : SV_Position;
};

[shader("vertex")]
Coords vertexMain(VertexInput input)
{
    Coords output;

    float4 worldPos = mul(pc.model, float4(input.position, 1.0));
    output.clipPos = mul(pc.lightSpaceMatrix, worldPos);

    return output;
}

[shader("fragment")]
void fragmentMain()
{
    // Empty for depth-only shadow mapping
}