// --- Interface Structures ---

struct VertexInput
{
    float3 position : POSITION;
};

struct VertexOutput
{

};

struct Coords {
    float4 clipPos : SV_Position;
    float2 uv      : TEXCOORD0;
};

// --- Entry Points ---

[shader("vertex")]
Coords vertexMain(VertexInput input) {
    Coords output;

    output.clipPos = float4(input.position, 1.0);

    return output;
}

[shader("fragment")]
float4 fragmentMain(Coords input) : SV_Target {
    // Return a solid magenta color (standard for "empty" or "error" shaders)
    return float4(1.0, 0.0, 1.0, 1.0);
}