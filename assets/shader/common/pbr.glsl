float D(float NdotH, float roughness)// GGX normal distribution function
{
    float alpha = roughness * roughness;
    float alpha2 = alpha * alpha;
    float denom = NdotH * NdotH * (alpha2 - 1.0) + 1.0;
    return alpha2 / (PI * denom * denom);
}

float G1(float Ndot, float roughness)// GGX geometry function for one direction (either view or light)
{
    float alpha = roughness * roughness;
    float k = (alpha + 1.0) * (alpha + 1.0) / 8.0;
    return Ndot / (Ndot * (1.0 - k) + k);
}

float G(float NdotV, float NdotL, float alpha)// GGX geometry function for both view and light directions
{
    return G1(NdotV, alpha) * G1(NdotL, alpha);
}

vec3 FresnelSchlick(float cosTheta, vec3 F0)
{
    return F0 + (vec3(1.0) - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

vec3 EvaluateBRDF(vec3 N, vec3 V, vec3 L, vec3 radiance, vec3 albedo, float metallic, float roughness)
{
    float NdotL = max(dot(N, L), 0.0);
    float NdotV = max(dot(N, V), 0.0);
    if (NdotL <= 0.0 || NdotV <= 0.0)
    return vec3(0.0);
    
    vec3 H = normalize(V + L);
    float NdotH = max(dot(N, H), 0.0);
    float HdotV = max(dot(H, V), 0.0);
    
    vec3 F0 = mix(vec3(0.04), albedo, metallic);
    float Dv = D(NdotH, roughness);
    float Gv = G(NdotV, NdotL, roughness);
    vec3 F = FresnelSchlick(HdotV, F0);
    
    vec3 spec = (Dv * Gv * F) / max(4.0 * NdotV * NdotL, EPS);
    vec3 kS = F;
    vec3 kD = (vec3(1.0) - kS) * (1.0 - metallic);
    vec3 diffuse = kD * albedo / PI;
    
    return (diffuse + spec) * radiance * NdotL;
}