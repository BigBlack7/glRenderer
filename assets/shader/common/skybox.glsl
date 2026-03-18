const float SKY_PI = 3.14159265359;

// 输入方向向量uvw, 输出全景图uv
vec2 UvwToUv(vec3 uvw)
{
    vec3 d = normalize(uvw);
    float u = atan(d.z, d.x) / (2.0 * SKY_PI) + 0.5;
    float v = asin(clamp(d.y, - 1.0, 1.0)) / SKY_PI + 0.5;
    return vec2(u, v);
}