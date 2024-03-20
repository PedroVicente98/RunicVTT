#include "Marker.h"

Marker::Marker(const std::string& path)
    : index_buffer(nullptr, NULL), shader(0),texture(path), vertex_buffer(nullptr, NULL)
{
    float positions[] = {
            -1.0f, -1.0f, 0.0f, 0.0f,//0
             1.0f, -1.0f, 1.0f, 0.0f,//1
             1.0f, 1.0f,  1.0f, 1.0f,//2
             -1.0f, 1.0f, 0.0f, 1.0f //3
    };

    unsigned int indices[] = {
        0 , 1 , 2,
        2 , 3 , 0
    };

    GLCall(glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA));
    GLCall(glEnable(GL_BLEND));

    vertex_array = VertexArray();
    vertex_buffer = VertexBuffer(positions, 4/*number of vertexes*/ * 4/*floats per vertex*/ * sizeof(float));

    VertexBufferLayout layout;
    layout.Push<float>(2);
    layout.Push<float>(2);
    vertex_array.AddBuffer(vertex_buffer, layout);

    index_buffer = IndexBuffer(indices, 6);

    glm::mat4 proj = glm::ortho(-1.0f, 1.0f, -1.0f, 1.0f, 1.0f, -1.0f);

    shader = Shader("res/shaders/Basic.shader");
    shader.Bind();
    //shader.SetUniform4f("u_Color", 0.8f, 0.3f, 0.8f, 1.0f);
    shader.SetUniform1i("u_Texture", 0);
    shader.SetUniformMat4f("u_MVP", proj);

    //Texture texture("res/texture/PhandalinBattlemap2.png");
    texture = Texture(path);
    //Texture texture("\\res\\texture\\PhandalinBattlemap2.png");
    texture.Bind();

    vertex_array.Unbind();
    vertex_buffer.Unbind();
    index_buffer.Unbind(); 
    shader.Unbind();
}

Marker::~Marker()
{
}
