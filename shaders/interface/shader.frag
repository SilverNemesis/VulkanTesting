#version 450 core
#extension GL_ARB_separate_shader_objects : enable

layout(binding=0) uniform sampler2D sTexture;

layout(location = 0) in struct {
    vec4 Color;
    vec2 UV;
} In;

layout(location = 0) out vec4 fColor;

void main()
{
    fColor = In.Color * texture(sTexture, In.UV.st).r;
}
