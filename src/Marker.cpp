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
/*

#include <iostream>
#include <fstream>
#include <glm/glm.hpp>
#include <string>

// Serialize primitive types and complex types like glm::vec2 and std::string
void serialize(std::ofstream& out, const glm::vec2& vec) {
    out.write(reinterpret_cast<const char*>(&vec.x), sizeof(vec.x));
    out.write(reinterpret_cast<const char*>(&vec.y), sizeof(vec.y));
}

void serialize(std::ofstream& out, const std::string& str) {
    size_t length = str.size();
    out.write(reinterpret_cast<const char*>(&length), sizeof(length));  // Serialize string length
    out.write(str.data(), length);  // Serialize string data
}

void serialize(std::ofstream& out, const int& value) {
    out.write(reinterpret_cast<const char*>(&value), sizeof(value));
}

void serialize(std::ofstream& out, const float& value) {
    out.write(reinterpret_cast<const char*>(&value), sizeof(value));
}



void deserialize(std::ifstream& in, glm::vec2& vec) {
    in.read(reinterpret_cast<char*>(&vec.x), sizeof(vec.x));
    in.read(reinterpret_cast<char*>(&vec.y), sizeof(vec.y));
}

void deserialize(std::ifstream& in, std::string& str) {
    size_t length;
    in.read(reinterpret_cast<char*>(&length), sizeof(length));  // Deserialize string length
    str.resize(length);  // Resize string to fit data
    in.read(&str[0], length);  // Deserialize string data
}

void deserialize(std::ifstream& in, int& value) {
    in.read(reinterpret_cast<char*>(&value), sizeof(value));
}

void deserialize(std::ifstream& in, float& value) {
    in.read(reinterpret_cast<char*>(&value), sizeof(value));
}


int main() {
    // Data to serialize
    glm::vec2 position(10.0f, 20.0f);
    std::string name = "Player";
    int score = 100;
    float health = 99.5f;

    // Serialize the data to a file
    std::ofstream out("savegame.bin", std::ios::binary);
    serialize(out, position);
    serialize(out, name);
    serialize(out, score);
    serialize(out, health);
    out.close();

    // Data to deserialize into
    glm::vec2 loaded_position;
    std::string loaded_name;
    int loaded_score;
    float loaded_health;

    // Deserialize the data from the file
    std::ifstream in("savegame.bin", std::ios::binary);
    deserialize(in, loaded_position);
    deserialize(in, loaded_name);
    deserialize(in, loaded_score);
    deserialize(in, loaded_health);
    in.close();

    // Output the deserialized data
    std::cout << "Position: (" << loaded_position.x << ", " << loaded_position.y << ")\n";
    std::cout << "Name: " << loaded_name << "\n";
    std::cout << "Score: " << loaded_score << "\n";
    std::cout << "Health: " << loaded_health << "\n";

    return 0;
}


*/
