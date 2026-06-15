[[vk::binding(0, 0)]] RWTexture2D<float4> outputImage;

struct PushConstants {
    float time;
    float2 mousePos;
};
[[vk::push_constant]] ConstantBuffer<PushConstants> pc;


// --- ShaderToy Macros translated to Slang ---

// The T(x) macro: (abs(.5 - fract(x)) - .25)
// Overloaded to handle float, float3, and float4 seamlessly
float  T(float x)  { return abs(0.5 - frac(x)) - 0.25; }
float3 T(float3 x) { return abs(0.5 - frac(x)) - 0.25; }
float4 T(float4 x) { return abs(0.5 - frac(x)) - 0.25; }

// The R(t) macro: mat2(cos(t + vec4(0,11,33,0)))
// This is a clever GLSL trick to create a 2D rotation matrix.
// We translate it into a standard 2D rotation multiplier for HLSL.
float2 mulRot(float2 v, float t) {
    float c = cos(t);
    float s = sin(t);
    // Equivalent to GLSL: v * mat2(c, s, -s, c)
    return float2(v.x * c - v.y * s, v.x * s + v.y * c);
}

// --- Main Compute Shader ---

[shader("compute")]
[numthreads(16, 16, 1)]
void compMain(uint3 id : SV_DispatchThreadID) {
    uint width, height;
    outputImage.GetDimensions(width, height);
    if (id.x >= width || id.y >= height) return;

    // ShaderToy uses Bottom-Left origin, Vulkan uses Top-Left.
    // We flip the Y coordinate here so the math matches Shadertoy exactly.
    float2 I = float2(id.x, (height - 1) - id.y);

    // vec3 v = iResolution;
    float3 v = float3(float(width), float(height), 1.0);

    // v.xy = (I+I-v.xy)/v.x;
    v.xy = (I * 2.0 - v.xy) / v.x;

    float z = 0.0;
    float4 O = float4(0.0, 0.0, 0.0, 0.0);

    // The core accumulation loop
    for (float i = 0.0; i < 70.0; i++) {
        float3 p = z * v / 4.0;
        float n = pc.time / 64.0;

        // p.yz *= R(z/16. * T(n-.25));
        p.yz = mulRot(p.yz, (z / 16.0) * T(n - 0.25));

        p.z += n * 200.0;

        // p.xy *= R(z/4.*T(n));
        p.xy = mulRot(p.xy, (z / 4.0) * T(n));

        float3 w = ceil(p * 8.0) / 8.0;

        for (float n_inner = 1.0; n_inner < 4.0; n_inner += n_inner) {
            w += T(w.yzx * n_inner + p.z * 0.068) / n_inner;
        }

        float3 a = abs(w);
        n = length(a.xy - 0.8);
        z += min(n, 2.5 - max(a.x, a.y));

        // O += abs(T(length(p-w) + vec4(n,8,13,0)/19.)) / n
        O += abs(T(length(p - w) + float4(n, 8.0, 13.0, 0.0) / 19.0)) / n;
    }

    // O = tanh(O / 36.);
    O = tanh(O / 36.0);

    // Write out to Vulkan texture (Forcing Alpha to 1.0 for swapchain safety)
    outputImage[id.xy] = float4(O.xyz, 1.0);
}