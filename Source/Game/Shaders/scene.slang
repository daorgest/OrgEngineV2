struct Vertex
{
    float3 position;
    float3 normal;
    float4 tangent;
    float3 color;
    float2 uv;
};

struct VertexOutput
{
    float4 position      : SV_Position;
    float3 worldNormal   : NORMAL;
    float4 worldTangent  : TANGENT;
    float3 worldPosition : WORLDPOS;
    float3 color         : COLOR0;
    float2 texCoord      : TEXCOORD0;
};

enum LightTypes { Directional, Point, Spot };

struct Light
{
    float3 position;    float range;
    float3 direction;   float innerCone;
    float3 color;       float intensity;
    uint type;          float outerCone;
};

struct CameraUBO
{
    float3 camPosition;
    float  nearPlane;
    float  farPlane;
};

struct SceneUBO
{
    float4x4 view;
    float4x4 proj;
};

struct LightMeta
{
    uint lightCount;
};

// Scene Options
struct DebugUBO
{
    int   debugViewMode;
    float debugDepthRange;
    uint  disableNormalMap;
    uint  disableSpecular;
    float iblStrength;
    float iblRoughnessMipBias;
    float ambientStrength;
    float aoStrength;
    float metallicReflectScale;
    float roughnessReflectScale;
};

[[vk::binding(2, 0)]] ConstantBuffer<DebugUBO>   debugUBO;
[[vk::binding(3, 0)]] ConstantBuffer<CameraUBO>  cameraUBO;
[[vk::binding(4, 0)]] StructuredBuffer<Light>    lights;
[[vk::binding(5, 0)]] ConstantBuffer<LightMeta>  lightMeta;
[[vk::binding(6, 0)]] ConstantBuffer<SceneUBO>   sceneUBO;

[[vk::binding(0, 1)]] Sampler2D albedoTexture;
[[vk::binding(1, 1)]] Sampler2D normalTexture;

[[vk::binding(0, 2)]] SamplerCube envCubemap;

struct PushConstants
{
    float4x4 model;
    float3x3 normalMatrix;
    uint     vertexOffset;
    uint64_t vertexBufferAddress;
    float    roughness;
    float    metallic;
    float3   baseColorTint;
    float    _padding;
};
[[vk::push_constant]] ConstantBuffer<PushConstants> pc;

// Helper Functions
static const float PI = 3.14159265359;

float pow5(float x)
{
    return x * x * x * x * x;
}

float3 DecodeTangentSpaceNormal(float3 rgb)
{
    return normalize(rgb * 2.0 - 1.0);
}

float SmoothRangeFalloff(float dist, float range)
{
    float t = saturate(1.0 - dist / max(range, 1e-4));
    // Use Hermite polynomial smoothstep(t)
    return t * t * (3.0 - 2.0 * t);
}

float LinearizeDepth(float d, float n, float f)
{
    // Convert to NDC space [-1,1]
    float zNDC = d * 2.0 - 1.0;
    // Solve projection eq. for view-space Z
    return (2.0 * n * f) / (f + n - zNDC * (f - n));
}

// PBR Functions
float DistributionGGX(float3 N, float3 H, float alpha)
{
    float a2 = alpha * alpha;
    float NdotH = saturate(dot(N, H));
    float denom = (NdotH * NdotH) * (a2 - 1.0) + 1.0;
    return a2 / max(PI * denom * denom, 1e-6);
}

float GeometrySchlickGGX(float NdotV, float roughness)
{
    float r = roughness + 1.0;
    float k = (r * r) * 0.125; // (r^2)/8
    return NdotV / max(NdotV * (1.0 - k) + k, 1e-6);
}

float GeometrySmith(float3 N, float3 V, float3 L, float roughness)
{
    float NdotV = saturate(dot(N, V));
    float NdotL = saturate(dot(N, L));
    return GeometrySchlickGGX(NdotV, roughness) * GeometrySchlickGGX(NdotL, roughness);
}

float3 fresnelSchlick(float cosTheta, float3 F0)
{
    float m = 1.0 - cosTheta;
    float m2 = m * m;
    float m5 = m2 * m2 * m; // same as pow(m,5)
    return F0 + (1.0 - F0) * m5;
}

// ACES Filmic Tone Mapping (Narkowicz 2015 approximation)
// with Rec.709 gamma output

inline float3 TonemapACES(float3 color)
{
    // Input transform
    const float3x3 ACESInputMat = float3x3(
        0.59719, 0.35458, 0.04823,
        0.07600, 0.90834, 0.01566,
        0.02840, 0.13383, 0.83777);

    // Output transform
    const float3x3 ACESOutputMat = float3x3(
        1.60475, -0.53108, -0.07367,
        -0.10208,  1.10813, -0.00605,
        -0.00327, -0.07276,  1.07602);

    float3 x = mul(ACESInputMat, color);

    float3 a = x * (x + 0.0245786.xxx) - 0.000090537.xxx;
    float3 b = x * (0.983729.xxx * x + 0.4329510.xxx) + 0.238081.xxx;
    float3 y = a / b;

    // Back to RGB
    color = mul(ACESOutputMat, y);

    // Clamp & gamma correct
    color = saturate(color);
    color = pow(color, 1.0 / 2.2);

    return color;
}


// Vertex Shader
[shader("vertex")]
VertexOutput vertexMain(uint vertexID : SV_VertexID)
{
    VertexOutput o;

    Ptr<Vertex> verts = Ptr<Vertex>(pc.vertexBufferAddress);
    Vertex v = verts[pc.vertexOffset + vertexID];

    float4x4 M = pc.model;
    float3x3 nM = pc.normalMatrix;

    float4 posWS = mul(M, float4(v.position, 1.0));
    o.worldPosition = posWS.xyz;

    float3 N = normalize(mul(nM, v.normal));
    float3 T = mul(nM, v.tangent.xyz);

    T = normalize(T - N * dot(N, T));

    o.worldNormal = N;
    o.worldTangent = float4(T, v.tangent.w);

    o.color = v.color;
    o.texCoord = v.uv;

    float4 posVS = mul(sceneUBO.view, posWS);
    o.position = mul(sceneUBO.proj, posVS);

    return o;
}

// Fragment Shader
[shader("fragment")]
float4 fragmentMain(VertexOutput input, float4 fragCoord : SV_Position) : SV_Target
{
    // Debug Views
    switch (debugUBO.debugViewMode)
    {
        case 3: return float4(fragCoord.zzz, 1.0);

        case 4:
        {
            float dLin = LinearizeDepth(fragCoord.z, cameraUBO.nearPlane, cameraUBO.farPlane);
            float dN = saturate(dLin / max(debugUBO.debugDepthRange, 1e-6));
            return float4(dN.xxx, 1.0);
        }
        case 5:
        {
            float dLin = LinearizeDepth(fragCoord.z, cameraUBO.nearPlane, cameraUBO.farPlane);
            float dN = log2(1.0 + dLin) / max(log2(1.0 + debugUBO.debugDepthRange), 1e-6);
            return float4(dN.xxx, 1.0);
        }
        case 6:
        {
            float2 uvw = frac(input.texCoord);
            return float4(uvw, 0.0, 1.0);
        }

        default: break;
    }

    
    // Material Setup
    float3 albedo = albedoTexture.Sample(input.texCoord).rgb * input.color.rgb * pc.baseColorTint;

    // TBN Matrix (re-orthogonalized in fragment shader)
    float3 N = normalize(input.worldNormal);
    float3 T = normalize(input.worldTangent.xyz - N * dot(N, input.worldTangent.xyz));
    float3 B = cross(N, T) * input.worldTangent.w;

    float3x3 TBN = float3x3(T, B, N);

    // Normal Mapping
    if (debugUBO.disableNormalMap == 0)
    {
        float3 nTex = normalTexture.Sample(input.texCoord).rgb;
        if (dot(nTex - 0.5.xxx, nTex - 0.5.xxx) > 1e-4)
            N = normalize(mul(DecodeTangentSpaceNormal(nTex), TBN));
    }


    // PBR Setup
    float3 V = normalize(cameraUBO.camPosition - input.worldPosition);
    float roughness = clamp(pc.roughness, 0.025, 1.0);
    float metallic = saturate(pc.metallic);
    float3 F0 = lerp(float3(0.04, 0.04, 0.04), albedo, metallic);
    float alpha = roughness * roughness;


    // Direct Lighting
    float3 Lo = 0.0;

    [loop]
    for (uint i = 0; i < lightMeta.lightCount; i++)
    {
        Light light = lights[i];
        float3 L;
        float attenuation = 1.0;

        if (light.type == LightTypes.Directional)
        {
            L = normalize(-light.direction);
        }
        else
        {
            float3 lightToPixel = light.position - input.worldPosition;
            float distance = length(lightToPixel);
            L = lightToPixel / max(distance, 1e-4);

            attenuation = 1.0 / max(distance * distance, 1e-6);
            attenuation *= SmoothRangeFalloff(distance, light.range);

            if (light.type == LightTypes.Spot)
            {
                float3 spotDir = normalize(light.direction);
                float theta = dot(L, -spotDir);
                float epsilon = light.innerCone - light.outerCone;
                float intensity = saturate((theta - light.outerCone) / max(epsilon, 1e-5));
                intensity = intensity * intensity * (3.0 - 2.0 * intensity);
                attenuation *= intensity;
            }
        }

        float3 H = normalize(V + L);
        float3 radiance = light.color * light.intensity * attenuation;

        // Cook-Torrance BRDF
        float NDF = DistributionGGX(N, H, alpha);
        float G = GeometrySmith(N, V, L, roughness);
        float3 F = fresnelSchlick(max(dot(H, V), 0.0), F0);

        float3 numerator = NDF * G * F;
        float NdotV = max(dot(N, V), 0.0);
        float NdotL = max(dot(N, L), 0.0);
        float denominator = 4.0 * NdotV * NdotL + 0.0001;
        float3 specular = numerator / denominator;

        float3 kS = F;
        float3 kD = float3(1.0, 1.0, 1.0) - kS;
        kD *= 1.0 - metallic;

        if (debugUBO.disableSpecular != 0)
            specular = float3(0.0, 0.0, 0.0);

        Lo += (kD * albedo / PI + specular) * radiance * NdotL;
    }

    // Debug Views (post-lighting)
    if (debugUBO.debugViewMode == 1)
        return float4(albedo, 1.0);

    if (debugUBO.debugViewMode == 2)
        return float4(N * 0.6 + 0.5, 1.0);

    // IBL
    float ao = debugUBO.aoStrength;
    float3 ambient = float3(debugUBO.ambientStrength, debugUBO.ambientStrength, debugUBO.ambientStrength) * albedo * ao;

    float3 R = reflect(-V, N);
    float maxMipLevel = 6.0;
    float mipLevel = saturate(roughness + debugUBO.iblRoughnessMipBias) * maxMipLevel;
    float3 envColor = envCubemap.SampleLevel(R, mipLevel).rgb;

    float3 F_ambient = fresnelSchlick(max(dot(N, V), 0.0), F0);
    float iblContribution = metallic * debugUBO.metallicReflectScale * (1.0 - roughness * debugUBO.roughnessReflectScale);
    float3 specularIBL = envColor * F_ambient * iblContribution * debugUBO.iblStrength;

    float3 color = ambient + Lo + specularIBL * ao;
    color = TonemapACES(color);
    return float4(color, 1.0);
}

