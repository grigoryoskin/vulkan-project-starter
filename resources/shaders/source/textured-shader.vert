#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(binding = 0) uniform UniformBufferObject {
    mat4 model;
} ubo;

layout(binding = 2) uniform SharedUniformBufferObject {
    mat4 view;
    mat4 proj;
    vec4 lightPos;
} sharedUbo;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexColor;

layout(location = 0) out vec3 outNormal;
layout(location = 1) out vec2 outTexColor;
layout(location = 2) out vec3 outWorldPos;
layout(location = 3) out vec3 outLightPos;

void main() {
    vec4 worldPos = ubo.model * vec4(inPosition, 1.0);
    gl_Position = sharedUbo.proj * sharedUbo.view * worldPos;
    outNormal = mat3(transpose(inverse(ubo.model))) * inNormal;
    outLightPos = sharedUbo.lightPos.xyz;
    outTexColor = inTexColor;
    outWorldPos = worldPos.xyz;
}
