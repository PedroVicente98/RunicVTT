#shader vertex
#version 330 core

layout(location = 0) in vec4 position;
layout(location = 1) in vec2 textCoord;

out vec2 v_TextCoord;

uniform mat4 u_MVP;

void main()
{
    gl_Position = u_MVP * position;
    v_TextCoord = textCoord;
};


#shader fragment
#version 330 core

out vec4 color;

in vec2 v_TextCoord;

//uniform vec4 u_Color;
uniform sampler2D u_Texture;
uniform float u_Alpha;

void main()
{
    vec4 textColor = texture(u_Texture, v_TextCoord);
    textColor.a = textColor.a * u_Alpha;
    color = textColor;

};