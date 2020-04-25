#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(binding = 0) uniform Camera {
    mat4 view;
    mat4 proj;
} camera;

layout(binding = 1) uniform Model {
    mat4 model;
} model;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec2 inTexCoord;

layout(location = 0) out vec2 fragTexCoord;

void main() {
    gl_Position = camera.proj * camera.view * model.model * vec4(inPosition, 1.0);
    fragTexCoord = inTexCoord;
}