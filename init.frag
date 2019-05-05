#version 450
#extension GL_ARB_separate_shader_objects : enable

struct Position
{
	float x, y;
};

layout(binding = 0) buffer Positions
{
	uint count;
	Position pos[];
} positions;

layout(location = 0) out vec4 outColor;

void main() {
    outColor = vec4(0,0,0,0);

	for(int i = 0; i < positions.count; i++)
	{
		vec2 c = vec2(positions.pos[i].x, positions.pos[i].y);
		vec2 f = gl_FragCoord.xy - vec2(0.5, 0.5);
		if(length(c - f) < 0.1)
		{
			outColor = vec4(1, 1, 1, 1);
			break;
		}
	}
}