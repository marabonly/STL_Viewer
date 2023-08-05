#shader vertex
#version 330 core

layout(location = 0) in vec4 position;

uniform mat4 view;

// Feedback transform outputs
out vec3 posXYZ;		// Transformed coordinates

void main()
{
	gl_Position = view * position;

	posXYZ = gl_Position.xyz;
};
