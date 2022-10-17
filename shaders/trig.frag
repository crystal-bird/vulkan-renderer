
#version 460

layout(location = 0) in vec3 vNormal;
layout(location = 1) in vec2 vTexCoord;

layout(location = 0) out vec4 fColor;

void main()
{
    fColor = vec4(1.0, 0.0, 1.0, 1.0);
}
