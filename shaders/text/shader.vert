#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(binding = 0) uniform UniformBufferObject {
    mat4 proj;
} ubo;

layout(push_constant) uniform PushConstants {
  vec3 color;
  vec2 position;
} pushConstants;

layout(location = 0) in vec2 inPosition;
layout(location = 1) in vec2 inTexCoord;

layout(location = 0) out vec2 fragTexCoord;

void main() {
    gl_Position = ubo.proj * vec4(inPosition + pushConstants.position, 0.0, 1.0);
    fragTexCoord = inTexCoord;
}