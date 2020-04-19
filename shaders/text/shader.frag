#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(binding = 0) uniform sampler2D texSampler;

layout(push_constant) uniform PushConstants {
  vec3 fragColor;
} pushConstants;

layout(location = 0) in vec2 fragTexCoord;

layout(location = 0) out vec4 outColor;

void main() {
    outColor = vec4(pushConstants.fragColor, texture(texSampler, fragTexCoord).r);
}