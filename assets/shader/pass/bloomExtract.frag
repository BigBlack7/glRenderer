#version 460 core

in vec2 oUV;
out vec4 oPixelColor;

uniform sampler2D uSceneSampler;
uniform float uThreshold = 1.0;
uniform float uKnee = 0.5;

void main()
{
    vec3 hdr = max(texture(uSceneSampler, oUV).rgb, vec3(0.0));
    float brightness = max(max(hdr.r, hdr.g), hdr.b);
    
    /*
        weight
        1.0 |           ╭──────────────────
            |          /
            |         /
            |        /
        0.5 |       ╱
            |      ╱
            |     ╱
            |    ╱
        0.0 |___╱_________________________ brightness
            0.5 0.75 1.0 1.25 1.5 2.0
               ↑    ↑   ↑    ↑   ↑
              开始 中间 阈值 中间 结束
              过渡 过渡      过渡 过渡
    */
    
    // 软阈值
    float knee = max(uKnee, 0.000001);
    float x = brightness - uThreshold;
    float soft = clamp((x + knee) / (2.0 * knee), 0.0, 1.0);
    float softWeight = soft * soft;
    float hardWeight = step(uThreshold, brightness);
    float weight = max(hardWeight, softWeight);
    
    oPixelColor = vec4(hdr * weight, 1.0);
}