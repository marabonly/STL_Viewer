#shader vertex
#version 330 core

layout(location = 0) in vec4 position;

uniform mat4 proj;
uniform mat4 view;

void main()
{
	gl_Position = proj * view * position;
};

#shader fragment
#version 330 core

layout(location = 0) out vec4 outColor;

uniform vec4 inColor;

void main()
{
	outColor = inColor;
};
