#version 460 core
out vec4 oPixelColor;

// in
in vec2 oUV;
in vec3 oNormal;
in vec3 oFragPos;

// sampler
uniform sampler2D uAlbedoSampler;

// uniform
uniform vec3 uViewPos;
uniform uint uMaterialFlags;
uniform vec3 uDefaultColor;
uniform float uShininess;

const uint MAT_HAS_ALBEDO_TEX = (1u << 0u);

void main()
{
    // common
    vec3 N = normalize(oNormal);
    vec3 V = normalize(uViewPos - oFragPos);
    vec3 object_color = ((uMaterialFlags & MAT_HAS_ALBEDO_TEX) != 0u) ? texture(uAlbedoSampler, oUV).rgb : uDefaultColor;
    
    // directional light
    vec3 Ld = normalize(vec3(-1.0, - 1.0, - 1.0)); // 光线方向（世界空间）
    vec3 L = normalize(-Ld); // 片元指向光源方向
    vec3 light_color = vec3(0.9255, 0.902, 0.8824);
    
    // phong shading model
    float diff = max(dot(N, L), 0.0);
    vec3 diffuse_color = light_color * object_color * diff;
    
    vec3 R = reflect(-L, N);
    float spec = pow(max(dot(R, V), 0.0), uShininess);
    vec3 specular_color = light_color * spec * step(0.0, diff);
    
    vec3 ambient_color = vec3(0.1) * object_color;
    
    oPixelColor = vec4(diffuse_color + specular_color + ambient_color, 1.0);
}