#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(binding = 0) uniform SharedUniformBufferObject {
    mat4 view;
    mat4 proj;
    vec4 lightPos;
} sharedUbo;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexColor;

void main() {
    vec4 worldPos = vec4(inPosition, 1) + sharedUbo.lightPos;
    gl_Position = sharedUbo.proj * sharedUbo.view * worldPos;
}
