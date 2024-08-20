//#shader vertex
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


//#shader fragment
#version 330 core

out vec4 color;

in vec2 v_textcoord;

uniform vec4 u_color;
uniform sampler2d u_texture;

void main()
{
    vec4 textcolor = texture(u_texture, v_textcoord);
    color = textcolor;

};