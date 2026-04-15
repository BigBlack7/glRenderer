#version 460 core
in vec3 oLocalDir;
out vec4 oPixelColor;

uniform samplerCube uEnvironmentMap;

const float PI = 3.14159265359;

void main()
{
    vec3 N = normalize(oLocalDir);
    vec3 up = vec3(0.0, 1.0, 0.0);
    vec3 right = normalize(cross(up, N));
    up = normalize(cross(N, right));
    
    vec3 irradiance = vec3(0.0);
    float sampleDelta = 0.025;
    float sampleCount = 0.0;
    
    for(float phi = 0.0; phi < 2.0 * PI; phi += sampleDelta)
    {
        for(float theta = 0.0; theta < 0.5 * PI; theta += sampleDelta)
        {
            vec3 tangentSample = vec3(sin(theta) * cos(phi), sin(theta) * sin(phi), cos(theta));
            vec3 sampleVec = tangentSample.x * right + tangentSample.y * up + tangentSample.z * N;
            
            irradiance += texture(uEnvironmentMap, sampleVec).rgb * cos(theta) * sin(theta);
            sampleCount += 1.0;
        }
    }
    
    irradiance = PI * irradiance / max(sampleCount, 1.0);
    oPixelColor = vec4(irradiance, 1.0);
}
