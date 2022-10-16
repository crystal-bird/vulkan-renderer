
#version 460

const vec3 positions[] =
{
    vec3(-0.5, -0.5, 0.0),
    vec3( 0.0,  0.5, 0.0),
    vec3( 0.5, -0.5, 0.0),
};

void main()
{
    gl_Position = vec4(positions[gl_VertexIndex] * vec3(1.0, -1.0, 1.0), 1.0);
}
