
#version 460

struct Vertex
{
    float vx, vy, vz;
    float nx, ny, nz;
    float tu, tv;
};

layout(binding = 0) readonly buffer Vertices
{
    Vertex vertices[];
};

layout(location = 0) out vec3 vNormal;
layout(location = 1) out vec2 vTexCoord;
layout(location = 2) out vec4 vColor;

void main()
{
    Vertex v = vertices[gl_VertexIndex];

    vec3 position = vec3(v.vx, v.vy * -1.0, v.vz + 0.05);
    vec3 normal = vec3(v.nx, v.ny, v.nz);
    vec2 texcoord = vec2(v.tu, v.tv);

    gl_Position = vec4(position, 1.0);

    vNormal = normal;
    vTexCoord = texcoord;
    vColor = vec4(normal * 0.5 + 0.5, 1.0);
}
