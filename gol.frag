#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(binding = 0) uniform sampler2D samplerGol;

layout(location = 0) out vec4 outColor;

layout(origin_upper_left) in vec4 gl_FragCoord;

int cell(vec2 offset)
{
	vec2 f = gl_FragCoord.xy - vec2(0.5, 0.5);

	//if(f.x < 1 || f.x > 1365 || f.y < 1 || f.y > 767)
	//	return 1;

	vec4 color = texture(samplerGol, f + offset);

	if(color.a < 0.25)
		return 0;

	return 1;
}

void main() {
	vec2 f = gl_FragCoord.xy - vec2(0.5, 0.5);
	vec4 color = texture(samplerGol, f);
	//outColor = vec4(0, 0, 0, 0);

	int val = cell(vec2(-1, -1)) + cell(vec2(0, -1)) + cell(vec2(1, -1)) + 
				cell(vec2(-1, 0)) + cell(vec2(1, 0)) + 
				cell(vec2(-1, 1)) + cell(vec2(0, 1)) + cell(vec2(1, 1));

	if(color.a > 0.25)
	{
		outColor = val == 3 || val == 2 ? vec4(1, 1, 1, 1) : vec4(0, 0, 0, 0);
	}
	else
	{
		outColor = val == 3 ? vec4(1, 1, 1, 1) : vec4(0, 0, 0, 0);
	}

	
}