#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(binding = 0) buffer Positions
{
	uint count;
	uint width;
	uint pos[];
} positions;

layout(location = 0) out vec4 outColor;

void main() {
    outColor = vec4(0,0,0,0);

	vec2 f = gl_FragCoord.xy - vec2(0.5, 0.5);
	uint index = uint(f.y) * positions.width + uint(f.x);
	
	if(index < positions.count && positions.pos[index] > 0)
	{
		outColor = vec4(1, 1, 1, 1);
	}
}