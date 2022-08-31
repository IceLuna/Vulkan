
layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec2 a_UV;

layout(location = 0) out vec2 outUV;

layout(push_constant) uniform Constants
{
    mat4 model;
    mat4 view_proj;
};

void main()
{
    gl_Position = view_proj * model * vec4(a_Position, 1.0);
    outUV = a_UV;
}
