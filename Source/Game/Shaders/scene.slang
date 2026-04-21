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

    nointerpolation uint  materialIndex : MAT_INDEX;
    nointerpolation float instRoughness : ROUGHNESS; // Unique float
    nointerpolation float instMetallic  : METALLIC;  // Unique float
};

enum MaterialType : uint { Opaque, AlphaMask, Transparent };

struct MaterialProperties
{
	float4 baseColor;
	float3 emissive;
	float roughness = 0.5f;
	float metallic = 0.0f;
	float ior = 1.5f;

	uint albedoIndex = 0;
	uint normalIndex = 0;

	MaterialType type = MaterialType::Opaque;
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

struct GPUInstanceData
{
	float4x4 worldMatrix;
	uint materialIndex;
	float roughness;
	float metalic;
};

struct LightSceneData
{
    Light lights[16];
    uint lightCount;
};

// Scene Options
struct DebugUBO {
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

    // --- Toon Tuning Params ---
    float toonThreshold;
    float toonSmoothness;
    float rimThreshold;
    float rimSmoothness;
    float rimIntensity;
    float3 shadowTint;
};

[[vk::binding(2, 0)]] ConstantBuffer<DebugUBO>   debugUBO;
[[vk::binding(3, 0)]] ConstantBuffer<CameraUBO>  cameraUBO;
[[vk::binding(4, 0)]] ConstantBuffer<LightSceneData>  sceneLights;
[[vk::binding(6, 0)]] ConstantBuffer<SceneUBO>   sceneUBO;

[[vk::binding(0, 1)]] StructuredBuffer<MaterialProperties> materialBuffer;
[[vk::binding(0, 2)]] Sampler2D globalTextures[];
[[vk::binding(0, 3)]] SamplerCube envCubemap;
[[vk::binding(0, 4)]] StructuredBuffer<GPUInstanceData> instanceBuffer;

struct PushConstants
{
    float4x4 model;
    uint64_t vertexBufferAddress;
    uint     vertexOffset;
	uint     isInstanced;
	float    instRoughness;
	float    instMetallic;
	uint     materialIndex;
	uint     _padding; //
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
    float standardD = 1.0 - d;
    float zNDC = standardD * 2.0 - 1.0;
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
    return saturate(color);
}

/** * Calculates world-space normals while supporting non-uniform scaling without
 * the expensive inverse-transpose matrix.
 *
 * Logic: "Counter-stretching" the normal by the reciprocal of the squared scale.
 * Source: Optimized implementation derived from Unity URP SpaceTransforms and
 * Fabian Giesen (The ryg blog). See Eric Lengyel, "Foundations of Game Engine
 * Development, Vol 1" for the underlying matrix orthogonality proof.
 *
 * Downsides:
 * - Assumes basis vectors are orthogonal. Does NOT support skew or shear.
 * - Adds per-vertex overhead (3 dots, 3 reciprocals) vs. a pre-computed CPU matrix.
 * - Potential NaNs if an axis scale is zero (guarded by 1e-8).
 */
float3 CalculateWorldNormal(float3 localNormal, float4x4 worldMatrix)
{
    // 1. Extract the 3x3 basis vectors (X, Y, Z)
    float3x3 m3x3 = (float3x3)worldMatrix;

    // 2. Calculate the reciprocal of the squared scale for each axis
    // This replaces the expensive inverse-transpose operation
    float3 invSqScale;
    invSqScale.x = 1.0 / max(dot(m3x3[0], m3x3[0]), 1e-8);
    invSqScale.y = 1.0 / max(dot(m3x3[1], m3x3[1]), 1e-8);
    invSqScale.z = 1.0 / max(dot(m3x3[2], m3x3[2]), 1e-8);

    // 3. Reconstruct the world normal by "counter-stretching" the local normal
    float3 worldNormal = localNormal.x * invSqScale.x * m3x3[0] +
                         localNormal.y * invSqScale.y * m3x3[1] +
                         localNormal.z * invSqScale.z * m3x3[2];

    return normalize(worldNormal);
}

// Vertex Shader
[shader("vertex")]
VertexOutput vertexMain(uint vertexID : SV_VertexID, uint instanceID : SV_InstanceID)
{
    VertexOutput o;

    Ptr<Vertex> verts = Ptr<Vertex>(pc.vertexBufferAddress);
    Vertex v = verts[pc.vertexOffset + vertexID];

    float4x4 worldMatrix;
    float roughness;
    float metallic;
    uint matIdx;

    if (pc.isInstanced == 1)
    {
        GPUInstanceData data = instanceBuffer[instanceID];
        worldMatrix = data.worldMatrix;
        matIdx      = data.materialIndex + pc.materialIndex;
        roughness   = data.roughness;
        metallic    = data.metalic;
    }
    else
    {
        worldMatrix = pc.model;
        matIdx      = pc.materialIndex;
        roughness   = pc.instRoughness;
        metallic    = pc.instMetallic;
    }

    float4 posWS = mul(worldMatrix, float4(v.position, 1.0));
    o.worldPosition = posWS.xyz;
    o.position = mul(sceneUBO.proj, mul(sceneUBO.view, posWS));

    o.worldNormal = CalculateWorldNormal(v.normal, worldMatrix);

    float3x3 rotPart = (float3x3)worldMatrix;
    float3 T = normalize(mul(rotPart, v.tangent.xyz));

    T = normalize(T - o.worldNormal * dot(o.worldNormal, T));
    o.worldTangent  = float4(T, v.tangent.w);
    o.color         = v.color;
    o.texCoord      = v.uv;

    o.instRoughness = roughness;
    o.instMetallic  = metallic;
    o.materialIndex = matIdx;
    return o;
}

[shader("fragment")]
float4 fragmentMain(VertexOutput input, float4 fragCoord : SV_Position) : SV_Target
{
    MaterialProperties mat = materialBuffer[input.materialIndex];
    float4 albedoSample = globalTextures[mat.albedoIndex].Sample(input.texCoord);

    if (mat.type == MaterialType::AlphaMask && albedoSample.a < 0.5)
    {
        discard;
    }

    float3 albedo = albedoSample.rgb * mat.baseColor.rgb;

    float roughness = clamp(input.instRoughness * mat.roughness, 0.025, 1.0);
    float metallic  = saturate(input.instMetallic * mat.metallic);
    float alpha     = roughness * roughness;

    float3 N = normalize(input.worldNormal);
    float3 V = normalize(cameraUBO.camPosition - input.worldPosition);

    float NdotV = max(dot(N, V), 0.0001);
    float G_V = GeometrySchlickGGX(NdotV, roughness);
    float specPreCalc = G_V / (4.0 * NdotV);
    switch (debugUBO.debugViewMode)
    {
        case 1: return float4(albedo, 1.0);
        case 3:
        {
            float dLin = LinearizeDepth(fragCoord.z, cameraUBO.nearPlane, cameraUBO.farPlane);
            float dN = saturate(dLin / max(debugUBO.debugDepthRange, 1e-6));
            return float4(dN.xxx, 1.0);
        }
        case 4: return float4(roughness.xxx, 1.0);
        case 5: return float4(metallic.xxx, 1.0);
        case 6: return float4(input.worldPosition * 0.1f, 1.0);
        case 7: return float4(frac(input.texCoord), 0.0, 1.0);
        default: break;
    }

    if (debugUBO.disableNormalMap == 0 && mat.normalIndex != 0)
    {
        float3 nTex = globalTextures[mat.normalIndex].Sample(input.texCoord).rgb;
        float3 T = normalize(input.worldTangent.xyz - N * dot(N, input.worldTangent.xyz));
        float3 B = cross(N, T) * input.worldTangent.w;
        float3x3 TBN = float3x3(T, B, N);

        if (dot(nTex - 0.5.xxx, nTex - 0.5.xxx) > 1e-4)
            N = normalize(mul(DecodeTangentSpaceNormal(nTex), TBN));
    }

    if (debugUBO.debugViewMode == 8)
    {
        float lod = globalTextures[mat.albedoIndex].CalculateLevelOfDetail(input.texCoord);
        // Visualize: Mip 0 is Black, Mip 10 is White
        return float4(lod.xxx / 10.0, 1.0);
    }

    if (debugUBO.debugViewMode == 2) return float4(N * 0.6 + 0.5, 1.0);

    // 4. Lighting Setup
    float3 F0 = lerp(float3(0.04), albedo, metallic);
    float3 Lo = 0.0;
    float maxToonDiffuse = 0.0;
    float toonSmoothInv = 1.0 / max(debugUBO.toonSmoothness, 1e-4);

    // 5. Direct Lighting Loop
    [loop]
    for (uint i = 0; i < sceneLights.lightCount; i++)
    {
        Light light = sceneLights.lights[i];
        float3 L;
        float attenuation = 1.0;

        // Light vector and attenuation math
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
                attenuation *= intensity * intensity * (3.0 - 2.0 * intensity);
            }
        }

        // Toon Diffuse logic
        float NdotL = dot(N, L);
        float halfLambert = NdotL * 0.5 + 0.5;
        float toonDiffuse = saturate((halfLambert - (debugUBO.toonThreshold - debugUBO.toonSmoothness)) * toonSmoothInv);
        maxToonDiffuse = max(maxToonDiffuse, toonDiffuse);

        // PBR Specular math
        float3 H = normalize(V + L);
        float3 radiance = light.color * light.intensity * attenuation;

        float NDF = DistributionGGX(N, H, alpha);
        float G   = GeometrySmith(N, V, L, roughness);
        float3 F  = fresnelSchlick(max(dot(H, V), 0.0), F0);

        float3 specular = (NDF * G * F * specPreCalc) / NdotL;
        if (debugUBO.disableSpecular != 0) specular = 0.0.xxx;

        float3 kD = (1.0.xxx - F) * (1.0 - metallic);

        // Accumulate final direct light
        Lo += (kD * albedo / PI + specular) * radiance * toonDiffuse;
    }

    // 6. Ambient & Shadow Tinting
    // We use maxToonDiffuse to determine if we are in shadow across ALL lights
    float3 shadowColor = albedo * debugUBO.shadowTint;
    float3 ambient = lerp(shadowColor, albedo, maxToonDiffuse) * debugUBO.ambientStrength * debugUBO.aoStrength;

    // 7. IBL Reflections
    float3 R = reflect(-V, N);
    float mipLevel = saturate(roughness + debugUBO.iblRoughnessMipBias) * 6.0;
    float3 envColor = envCubemap.SampleLevel(R, mipLevel).rgb;

    float3 F_ambient = fresnelSchlick(max(dot(N, V), 0.0), F0);
    float iblContribution = metallic * debugUBO.metallicReflectScale * (1.0 - roughness * debugUBO.roughnessReflectScale);
    float3 specularIBL = envColor * F_ambient * iblContribution * debugUBO.iblStrength;

    // 8. Toon Rim Light
    float rim = saturate(dot(V, N));
    float rimValue = smoothstep(debugUBO.rimThreshold - debugUBO.rimSmoothness,
                                debugUBO.rimThreshold + debugUBO.rimSmoothness,
                                rim);
    float3 finalRim = rimValue * debugUBO.rimIntensity * debugUBO.aoStrength;

    // 9. Final Composition
    float3 color = ambient + Lo + (specularIBL * debugUBO.aoStrength) + finalRim;

    return float4(TonemapACES(color), 1.0);
}