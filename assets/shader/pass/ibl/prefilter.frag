#version 460 core
in vec3 oLocalDir;
out vec4 oPixelColor;

uniform samplerCube uEnvironmentMap;
uniform float uRoughness;
uniform float uEnvMapResolution;

const float PI = 3.14159265359;
const float INV_UINT32 = 1.0 / 4294967296.0;

float RadicalInverseVdC(uint bits)
{
    bits = (bits << 16u)|(bits >> 16u);
    bits = ((bits & 0x55555555u) << 1u)|((bits & 0xAAAAAAAAu) >> 1u);
    bits = ((bits & 0x33333333u) << 2u)|((bits & 0xCCCCCCCCu) >> 2u);
    bits = ((bits & 0x0F0F0F0Fu) << 4u)|((bits & 0xF0F0F0F0u) >> 4u);
    bits = ((bits & 0x00FF00FFu) << 8u)|((bits & 0xFF00FF00u) >> 8u);
    return float(bits) * INV_UINT32;
}

vec2 Hammersley(uint i, uint n)
{
    return vec2(float(i) / float(n), RadicalInverseVdC(i));
}

vec3 ImportanceSampleGGX(vec2 xi, vec3 n, float roughness)
{
    float a = roughness * roughness;
    
    float phi = 2.0 * PI * xi.x;
    float cosTheta = sqrt((1.0 - xi.y) / (1.0 + (a * a - 1.0) * xi.y));
    float sinTheta = sqrt(max(1.0 - cosTheta * cosTheta, 0.0));
    
    vec3 h;
    h.x = cos(phi) * sinTheta;
    h.y = sin(phi) * sinTheta;
    h.z = cosTheta;
    
    vec3 up = abs(n.z) < 0.999 ? vec3(0.0, 0.0, 1.0) : vec3(1.0, 0.0, 0.0);
    vec3 tangent = normalize(cross(up, n));
    vec3 bitangent = cross(n, tangent);
    
    vec3 sampleVec = tangent * h.x + bitangent * h.y + n * h.z;
    return normalize(sampleVec);
}

float DistributionGGX(vec3 n, vec3 h, float roughness)
{
    float a = roughness * roughness;
    float a2 = a * a;
    float ndoth = max(dot(n, h), 0.0);
    float ndoth2 = ndoth * ndoth;
    
    float denom = ndoth2 * (a2 - 1.0) + 1.0;
    return a2 / max(PI * denom * denom, 0.0001);
}

void main()
{
    vec3 n = normalize(oLocalDir);
    vec3 r = n;
    vec3 v = r;
    
    const uint sampleCount = 1024u;
    vec3 prefiltered = vec3(0.0);
    float totalWeight = 0.0;
    float roughness = max(uRoughness, 0.04);
    float envArea = 6.0 * uEnvMapResolution * uEnvMapResolution;
    float saTexel = 4.0 * PI / max(envArea, 1.0);
    float maxMip = float(max(textureQueryLevels(uEnvironmentMap) - 1, 0));
    
    for(uint i = 0u; i < sampleCount; ++ i)
    {
        vec2 xi = Hammersley(i, sampleCount);
        vec3 h = ImportanceSampleGGX(xi, n, roughness);
        vec3 l = normalize(2.0 * dot(v, h) * h - v);
        
        float ndotl = max(dot(n, l), 0.0);
        if (ndotl > 0.0)
        {
            float d = DistributionGGX(n, h, roughness);
            float ndoth = max(dot(n, h), 0.0);
            float hdotv = max(dot(h, v), 0.0);
            float pdf = d * ndoth / max(4.0 * hdotv, 0.0001) + 0.0001;
            
            float saSample = 1.0 / (float(sampleCount) * pdf + 0.0001);
            float mipLevel = (roughness <= 0.04) ? 0.0 : 0.5 * log2(saSample / saTexel);
            mipLevel = clamp(mipLevel, 0.0, maxMip);
            
            prefiltered += textureLod(uEnvironmentMap, l, mipLevel).rgb * ndotl;
            totalWeight += ndotl;
        }
    }
    
    prefiltered = prefiltered / max(totalWeight, 0.0001);
    oPixelColor = vec4(prefiltered, 1.0);
}
