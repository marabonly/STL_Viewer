#shader vertex
#version 330 core

layout (location = 0) in vec4 position;

out vec4 vertexColor;
out vec2 TextureCoord;

void main()
{
	TextureCoord = position.zw;
	gl_Position = vec4(position.xy, -1.0, 1.0);
	vertexColor = vec4(0.5, 0.0, 0.0, 1.0);
};

#shader fragment
#version 330 core

out vec4 color;

in vec4 vertexColor;
in vec2 TextureCoord;

uniform sampler2D textureImage;

void main()
{
	color = texture(textureImage, TextureCoord);
	//color = vertexColor;
};
