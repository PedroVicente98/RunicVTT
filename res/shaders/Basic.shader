#shader vertex
#version 330 core

layout(location = 0) in vec4 position;
layout(location = 1) in vec2 textCoord;

out vec2 v_TextCoord;

//uniform mat4 u_MVP;

uniform mat4 projection;
uniform mat4 view;
uniform mat4 model;

void main()
{
    gl_Position = projection * view * model  * position;
    v_TextCoord = textCoord;
};


#shader fragment
#version 330 core

out vec4 color;

in vec2 v_TextCoord;

uniform int u_UseTexture;    // Flag to choose between texture and solid color
uniform sampler2D u_Texture;  // Texture for rendering (used if u_UseTexture is true)
uniform float u_Alpha;        // Alpha for transparency control

void main()
{
    // Conditional: Use texture or solid color
    if (u_UseTexture == 1)
    {
        // Sample the texture (map)
        vec4 textColor = texture(u_Texture, v_TextCoord);
        // Apply alpha transparency to the texture
        textColor.a *= u_Alpha;

        // Set the final color
        color = textColor;
    }
    else
    {
        // Use solid color with alpha blending
        vec4 solidColor = vec4(0.0, 0.0 ,0.0, 1.0);
        solidColor.a *= u_Alpha;

        // Set the final color to the solid color
        color = solidColor;
    }
}
