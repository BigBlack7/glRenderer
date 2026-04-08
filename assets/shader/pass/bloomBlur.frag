#version 460 core

in vec2 oUV;
out vec4 oPixelColor;

uniform sampler2D uSceneSampler;
uniform uint uHorizontal = 1u;

void main()
{
    vec2 texel = 1.0 / vec2(textureSize(uSceneSampler, 0)); // 像素在UV空间中的尺寸, 用于确定采样偏移量
    vec2 dir = (uHorizontal != 0u) ? vec2(texel.x, 0.0) : vec2(0.0, texel.y); // 确定模糊方向
    
    // G(x, y) = G(x)× G(y) 2D高斯函数可分离为两次1D, 将n²次采样优化为2n次采样
    vec3 c = texture(uSceneSampler, oUV).rgb * 0.227027;
    c += texture(uSceneSampler, oUV + dir * 1.0).rgb * 0.1945946;
    c += texture(uSceneSampler, oUV - dir * 1.0).rgb * 0.1945946;
    c += texture(uSceneSampler, oUV + dir * 2.0).rgb * 0.1216216;
    c += texture(uSceneSampler, oUV - dir * 2.0).rgb * 0.1216216;
    c += texture(uSceneSampler, oUV + dir * 3.0).rgb * 0.054054;
    c += texture(uSceneSampler, oUV - dir * 3.0).rgb * 0.054054;
    
    oPixelColor = vec4(c, 1.0);
}
