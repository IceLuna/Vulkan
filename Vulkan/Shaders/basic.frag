#version 450

layout (location = 0) in  vec3 inColor;
layout (location = 1) in  vec2 inTexCoords;

layout (binding = 1) uniform sampler2D s_Texture;

layout (location = 0) out vec4 outColor;

void main()
{
    outColor = texture(s_Texture, inTexCoords);
}
