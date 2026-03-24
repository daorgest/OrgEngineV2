[[vk::binding(0, 0)]] RWTexture2D<float4> outputImage;

struct PushConstants {
    float time;
    float2 mousePos; // Kept in struct to match your C++ side, but unused below
};
[[vk::push_constant]] ConstantBuffer<PushConstants> pc;

// --- Ported Helpers ---
float opSmoothUnion(float d1, float d2, float k) {
    float h = clamp(0.5 + 0.5 * (d2 - d1) / k, 0.0, 1.0);
    return lerp(d2, d1, h) - k * h * (1.0 - h);
}

float sdSphere(float3 p, float s) {
    return length(p) - s;
}

float map(float3 p) {
    float d = 2.0;

    for (int i = 0; i < 16; i++) {
        float fi = float(i);
        // Pure math-based movement
        float t = pc.time * (frac(fi * 412.531 + 0.513) - 0.5) * 2.0;

        float3 pos = sin(t + fi * float3(52.5126, 64.62744, 632.25)) * float3(2.0, 2.0, 0.8);

        float radius = lerp(0.5, 1.0, frac(fi * 412.531 + 0.5124));
        d = opSmoothUnion(sdSphere(p + pos, radius), d, 0.4);
    }
    return d;
}

float3 calcNormal(float3 p) {
    const float h = 1e-5;
    const float2 k = float2(1, -1);
    return normalize(k.xyy * map(p + k.xyy * h) +
                     k.yyx * map(p + k.yyx * h) +
                     k.yxy * map(p + k.yxy * h) +
                     k.xxx * map(p + k.xxx * h));
}

[shader("compute")]
[numthreads(16, 16, 1)]
void compMain(uint3 id : SV_DispatchThreadID) {
    uint width, height;
    outputImage.GetDimensions(width, height);
    if (id.x >= width || id.y >= height) return;

    // Standard UV setup with Y-flip for Vulkan
    float2 uv = float2(id.x, (height - 1) - id.y) / float2(width, height);
    float aspect = (float)width / (float)height;

    // Ray setup: screen size is 6m x 6m
    float3 rayOri = float3((uv - 0.5) * float2(aspect, 1.0) * 6.0, 3.0);
    float3 rayDir = float3(0.0, 0.0, -1.0);

    float depth = 0.0;
    float3 p = rayOri;

    // Raymarching
    for(int i = 0; i < 64; i++) {
        p = rayOri + rayDir * depth;
        float dist = map(p);
        depth += dist;
        if (dist < 1e-5) break;
    }

    depth = min(6.0, depth);
    float3 n = calcNormal(p);

    // Lighting
    float b = max(0.0, dot(n, float3(0.577, 0.577, 0.577)));
    float3 col = (0.5 + 0.5 * cos((b + pc.time * 3.0) + uv.xyx * 2.0 + float3(0, 2, 4))) * (0.85 + b * 0.35);
    col *= exp(-depth * 0.15);

    // Thickness/Alpha logic
    float alpha = 1.0 - (depth - 0.5) / 2.0;
    outputImage[id.xy] = float4(col, saturate(alpha));
}