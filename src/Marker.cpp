#include "Marker.h"

Marker::Marker(const std::string& texturePath, const glm::vec2& pos, const glm::vec2& sz, Renderer& rend)
    : position(pos), size(sz), texture(texturePath), renderer(rend), vb(nullptr, 0), ib(0, 0)
{
    float positions[] = {
        -1.0f, -1.0f, 0.0f, 0.0f,  // Bottom left
         1.0f, -1.0f, 1.0f, 0.0f,  // Bottom right
         1.0f,  1.0f, 1.0f, 1.0f,  // Top right
        -1.0f,  1.0f, 0.0f, 1.0f   // Top left
    };
    unsigned int indices[] = {
        0, 1, 2,
        2, 3, 0
    };

    vb = VertexBuffer(positions, 4 * 4 * sizeof(float));
    VertexBufferLayout layout;
    layout.Push<float>(2);  // Position
    layout.Push<float>(2);  // Texture coordinates
    va.AddBuffer(vb, layout);
    this->ib = IndexBuffer(indices, 6);

    glm::mat4 proj = glm::ortho(-1.0f, 1.0f, -1.0f, 1.0f, -1.0f, 1.0f);
    va.Unbind();
    vb.Unbind();
    ib.Unbind();
}


Marker::~Marker()
{
}

void Marker::draw(Shader& shader, const glm::mat4& viewProjMatrix) {
    shader.Bind();
    texture.Bind();

    // Calculate transformation matrix for the marker
    glm::mat4 model = glm::translate(glm::mat4(1.0f), glm::vec3(position, 0.0f));
    model = glm::scale(model, glm::vec3(size, 1.0f));

    // Combine with the view-projection matrix
    glm::mat4 mvp = viewProjMatrix * model;
    shader.SetUniformMat4f("u_MVP", mvp);
    shader.SetUniform1i("u_Texture", 0);

    // Assuming geometry setup is done, draw the quad
    renderer.Draw(va, ib, shader);  // Pass the specific VAO, IBO, Shader used by this marker
}