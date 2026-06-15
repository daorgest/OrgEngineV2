static const float PI = 3.14159265359;
#define MAX_LIGHTS 8
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
    nointerpolation float instRoughness : ROUGHNESS;
    nointerpolation float instMetallic  : METALLIC;
    nointerpolation uint  instanceID    : INSTANCE_ID;
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
    uint specularIndex = 0;

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
    Light lights[MAX_LIGHTS];
    uint lightCount;
};

struct PushConstants
{
    float4x4 model;
    uint64_t vertexBufferAddress;
    uint     vertexOffset;
	uint     isInstanced;
	float    instRoughness;
	float    instMetallic;
	uint     materialIndex;
};

// Scene Options
struct DebugUBO
{
    int   debugViewMode;
    float debugDepthRange;
    uint  disableNormalMap;
    float normalStrength;
    uint  disableSpecular;
    float specularStrength;

    float iblStrength;
    float iblRoughnessMipBias;
    float ambientStrength;
    float aoStrength;

    float metallicReflectScale;
    float roughnessReflectScale;
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

[[vk::push_constant]] ConstantBuffer<PushConstants> pc;


float pow5(float x)
{
    return x * x * x * x * x;
}

float3 DecodeTangentSpaceNormal(float3 rgb)
{
    return normalize(rgb * 2.0 - 1.0);
}

float LinearizeDepth(float d, float n, float f)
{
    return (n * f) / (n + d * (f - n));
}

// --- PBR Functions ---
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
    const float3x3 ACESInputMat = float3x3(
        0.59719, 0.35458, 0.04823,
        0.07600, 0.90834, 0.01566,
        0.02840, 0.13383, 0.83777);

    const float3x3 ACESOutputMat = float3x3(
        1.60475, -0.53108, -0.07367,
        -0.10208,  1.10813, -0.00605,
        -0.00327, -0.07276,  1.07602);

    float3 x = mul(ACESInputMat, color);
    float3 a = x * (x + 0.0245786.xxx) - 0.000090537.xxx;
    float3 b = x * (0.983729.xxx * x + 0.4329510.xxx) + 0.238081.xxx;
    float3 y = a / b;

    color = mul(ACESOutputMat, y);
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
    float3x3 m = (float3x3)worldMatrix;

    // Calculate the 3x3 Adjugate matrix
    // The adjugate is the transpose of the cofactor matrix.
    float3 adj0 = cross(m[1], m[2]);
    float3 adj1 = cross(m[2], m[0]);
    float3 adj2 = cross(m[0], m[1]);

    // Transpose is implicit in the construction of the adjugate matrix
    float3x3 adj = float3x3(adj0, adj1, adj2);

    // Transform the local normal by the adjugate
    return normalize(mul(adj, localNormal));
}

float CalculateLightAttenuation(Light light, float3 worldPos, out float3 L, out float distance)
{
    if (light.type == LightTypes.Directional)
    {
        L = normalize(-light.direction);
        distance = 0.0f;
        return 1.0f;
    }

    float3 pixelToLight = light.position - worldPos;
    distance = length(pixelToLight);
    L = pixelToLight / max(distance, 1e-4);

    float distanceSq = distance * distance;
    float attenuation = 1.0 / (distanceSq + 0.01);

    float rangeSq = max(light.range * light.range, 1e-4);
    float distRatioSq = distanceSq / rangeSq;
    float window = saturate(1.0 - (distRatioSq * distRatioSq));
    attenuation *= (window * window);

    if (light.type == LightTypes.Spot)
    {
        float3 spotDir = normalize(light.direction);
        float theta = dot(L, -spotDir);
        float epsilon = light.innerCone - light.outerCone;
        float spotIntensity = saturate((theta - light.outerCone) / max(epsilon, 1e-5));
        attenuation *= spotIntensity * spotIntensity * (3.0 - 2.0 * spotIntensity);
    }

    return attenuation;
}

// Vertex Shader
[shader("vertex")]
VertexOutput vertexMain(uint vertexID : SV_VertexID, uint instanceID : SV_InstanceID)
{
    VertexOutput o;

    Ptr<Vertex> verts = Ptr<Vertex>(pc.vertexBufferAddress);
    Vertex v = verts[vertexID + pc.vertexOffset];

    float4x4 worldMatrix;
    float roughness;
    float metallic;
    uint matIdx;

    if (pc.isInstanced == 2) // INDIRECT PATH
    {
        GPUInstanceData data = instanceBuffer[instanceID];
        worldMatrix = data.worldMatrix;
        matIdx      = pc.materialIndex;
        roughness   = data.roughness;
        metallic    = data.metalic;
    }
    else if (pc.isInstanced == 1) // INSTANCED PATH
    {
        GPUInstanceData data = instanceBuffer[instanceID];
        worldMatrix = mul(data.worldMatrix, pc.model);
        matIdx      = pc.materialIndex;
        roughness   = data.roughness;
        metallic    = data.metalic;
    }
    else // STANDARD PATH
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
    o.instanceID    = instanceID;
    return o;
}

[shader("fragment")]
float4 fragmentMain(VertexOutput input, float4 fragCoord : SV_Position) : SV_Target
{
    MaterialProperties mat = materialBuffer[input.materialIndex];
    float4 albedoSample = float4(1.0, 1.0, 1.0, 1.0);

    if (mat.albedoIndex > 2)
    {
        albedoSample = globalTextures[mat.albedoIndex].Sample(input.texCoord);
    }

    float metalMap = 1.0f;
    float roughMap = 1.0f;
    if (mat.specularIndex != 0 && debugUBO.disableSpecular == 0)
    {
        float4 packedPBR = globalTextures[mat.specularIndex].Sample(input.texCoord);
        // Roughness in Green and Metallic in Blue
        roughMap = packedPBR.g;
        metalMap = packedPBR.b;
    }

    if (mat.type == MaterialType::AlphaMask && albedoSample.a < 0.5)
    {
        discard;
    }

    float3 albedo = albedoSample.rgb * mat.baseColor.rgb;
    float roughness = clamp(input.instRoughness * mat.roughness * roughMap, 0.025, 1.0);
    float metallic  = saturate(input.instMetallic * mat.metallic * metalMap);

    if (debugUBO.debugViewMode == 9) // clay :3
    {
        albedo = float3(0.7, 0.7, 0.7); // Uniform grey
        metallic = 0.0;
        roughness = max(roughness, 0.3); // Ensure broad specular highlights
    }

    float alpha = roughness * roughness;
    float3 V = normalize(cameraUBO.camPosition - input.worldPosition);
    float3 geomNormal = normalize(input.worldNormal);

    // Construct orthonormal TBN basis
    float3 T = normalize(input.worldTangent.xyz);
    float3 N = normalize(geomNormal);
    T = normalize(T - N * dot(N, T));
    float tangentSign = (input.worldTangent.w < 0.0) ? -1.0 : 1.0;
    float3 B = cross(N, T) * tangentSign;
    float3x3 TBN = float3x3(T, B, N);

    if (debugUBO.disableNormalMap == 0 && mat.normalIndex != 1)
    {
        float3 nTex = globalTextures[mat.normalIndex].Sample(input.texCoord).rgb;
        float3 mapNormal = nTex * 2.0 - 1.0;

        float normalLen = length(mapNormal);

        // Calculate the Toksvig variance (guarding against divide-by-zero)
        float variance = saturate((1.0 - normalLen) / max(normalLen, 1e-5));
        float alpha = roughness * roughness;
        alpha += (variance * debugUBO.normalStrength);
        roughness = saturate(sqrt(alpha));


        mapNormal = mapNormal / max(normalLen, 1e-5);
        mapNormal.xy *= debugUBO.normalStrength;
        N = normalize(mul(mapNormal, TBN));
    }
    else
    {
        N = normalize(geomNormal);
    }

    switch (debugUBO.debugViewMode)
    {
        case 1: return float4(albedo, 1.0);
        case 2: return float4(N * 0.5 + 0.5, 1.0); // Shifted to 0.5 to properly visualize [-1, 1] range
        case 3:
        {
            float d = fragCoord.z;
            float n = cameraUBO.nearPlane;
            float f = cameraUBO.farPlane;
            float dLin = LinearizeDepth(d, n, f);

            float dVis = saturate(dLin / max(debugUBO.debugDepthRange, 1e-6));
            return float4(dVis.xxx, 1.0);
        }
        case 4: return float4(roughness.xxx, 1.0);
        case 5: return float4(metallic.xxx, 1.0);
        case 6: return float4(frac(input.worldPosition * 0.1f), 1.0); // now we get blocky
        case 7: return float4(frac(input.texCoord), 0.0, 1.0);
        case 8:
        {
            float lod = globalTextures[mat.albedoIndex].CalculateLevelOfDetail(input.texCoord);
            return float4(lod.xxx / 10.0, 1.0);
        }
        case 11: // Instance ID Randomizer (fun to look at)
        {
            float r = frac(sin(float(input.instanceID) * 12.9898) * 43758.5453);
            float g = frac(sin(float(input.instanceID) * 78.233) * 43758.5453);
            float b = frac(sin(float(input.instanceID) * 37.719) * 43758.5453);
            return float4(r, g, b, 1.0);
        }

        default: break;
    }

    float shadingNdotV = max(dot(N, V), 0.0001);
    float3 F0 = lerp(float3(0.04), albedo, metallic);
    float3 Lo = 0.0;

    [loop]
    for (uint i = 0; i < sceneLights.lightCount; i++)
    {
        Light light = sceneLights.lights[i];
        float3 L;
        float distance;

        float attenuation = CalculateLightAttenuation(light, input.worldPosition, L, distance);
        float rawNdotL = dot(N, L);
        float NdotL = saturate(rawNdotL);

        if (rawNdotL <= 0.0) continue;

        // Shadows disabled - fully lit analytical pass
        float shadowFactor = 1.0;

        float3 H = normalize(V + L);
        float3 radiance = light.color * light.intensity * attenuation * shadowFactor;

        float NDF = DistributionGGX(N, H, alpha);
        float G   = GeometrySmith(N, V, L, roughness);
        float3 F  = fresnelSchlick(max(dot(H, V), 0.0), F0);

        float3 specular = (NDF * G * F) / max(4.0 * shadingNdotV * NdotL, 1e-5);
        if (debugUBO.disableSpecular != 0) specular = 0.0.xxx;

        float3 kD = (1.0.xxx - F) * (1.0 - metallic);
        Lo += ((kD * albedo / PI) + specular) * radiance * NdotL;
    }

    float3 ambient = albedo * debugUBO.ambientStrength * debugUBO.aoStrength;

    float3 R = reflect(-V, N);
    float3 envColor = envCubemap.SampleLevel(R, saturate(roughness + debugUBO.iblRoughnessMipBias) * 6.0).rgb;
    float3 F_ambient = fresnelSchlick(max(dot(N, V), 0.0), F0);
    float3 specularIBL = envColor * F_ambient * (metallic * debugUBO.metallicReflectScale * (1.0 - roughness * debugUBO.roughnessReflectScale)) * debugUBO.iblStrength;

    float3 finalColor = ambient + Lo + (specularIBL * debugUBO.aoStrength);

    if (debugUBO.debugViewMode == 10) // if weird math with the PBR, this shows it!
    {
        // Flag NaNs or Infinity with Magenta
        if (any(isnan(finalColor)) || any(isinf(finalColor)))
            return float4(1.0, 0.0, 1.0, 1.0);

        // Flag severe negative lighting with Red
        if (any(finalColor < 0.0))
            return float4(1.0, 0.0, 0.0, 1.0);

        // Render valid pixels in grayscale so errors pop out
        float lum = dot(finalColor, float3(0.2126, 0.7152, 0.0722));
        return float4(lum.xxx, 1.0);
    }

    float pixelAlpha = albedoSample.a * mat.baseColor.a;
    return float4(TonemapACES(finalColor), pixelAlpha);
}