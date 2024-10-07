#version 450

vec2 verts[] ={
	vec2(-1,-1),
	vec2(-1, 1),
	vec2( 1,-1),
	vec2( 1, 1)
};

layout(location = 0)out vec2 texCoords; 
void main()
{
	gl_Position = vec4(verts[gl_VertexIndex], 0, 1);
	texCoords = verts[gl_VertexIndex];
}