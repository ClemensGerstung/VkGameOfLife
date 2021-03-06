#version 450
#extension GL_ARB_separate_shader_objects : enable

layout (location = 0) in vec3 inPos;
layout (location = 1) in vec2 inUV;

layout (set = 0, binding = 0) uniform Ubo {
	mat4 mat;
} ubo;

layout (location = 0) out vec2 outUV;

out gl_PerVertex {
	vec4 gl_Position;
};

void main() 
{
	outUV = inUV;

	gl_Position = ubo.mat * vec4(inPos.xy, 0.0, 1.0);
}