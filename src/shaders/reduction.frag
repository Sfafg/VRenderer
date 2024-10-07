#version 450
layout (binding = 0) uniform sampler2D inputImage;
layout (binding = 1, r32f) uniform writeonly image2D outputImage;

layout (local_size_x = 16, local_size_y = 16, local_size_z = 1) in;
void main()
{
	uvec2 pos = gl_GlobalInvocationID.xy;
	if(pos.x > imageSize.x || pos.y > imageSize.y)
		return;
		
	float depth = texture(inputImage, (vec2(pos) + vec2(0.5)) / imageSize).x;
	imageStore(outputImage, ivec2(pos), vec4(depth));
}