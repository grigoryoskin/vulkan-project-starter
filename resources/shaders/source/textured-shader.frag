#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec3 normal;
layout(location = 1) in vec2 fragTexCoord;
layout(location = 2) in vec3 worldPos;
layout(location = 3) in vec3 lightPos;

layout(binding = 2) uniform sampler2D texSampler;

layout(location = 0) out vec4 outColor;

void main() {
    float steps = 3;
    vec3 n = normalize(normal);
    vec3 l = lightPos - worldPos;
    float attenuation = 1/dot(l,l);
    float dif = max(dot(n, l) * attenuation, 0);
    // A little cell shading effect :)
    dif = 0.3 + floor(dif * steps)/steps;
    outColor = texture(texSampler, fragTexCoord) * dif;
}
