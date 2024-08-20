#include <GL/glew.h> 
#include <GLFW/glfw3.h>
#include "Board.h"

#include "Renderer.h"
#include "Shader.h"
#include "VertexBuffer.h"
#include "IndexBuffer.h"
#include "VertexArray.h"
#include "VertexBufferLayout.h"
#include "Texture.h"

#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"


//Board::Board(const std::string& board_name, const std::string& texturePath, ImVec2 cameraCenter, float initialZoom, int viewportWidth, int viewportHeight)
//	: board_name(board_name), mapTexture(texturePath), camera(cameraCenter, initialZoom, viewportWidth, viewportHeight) 
//{
//}

Board::Board(Renderer& rend, const std::string& texturePath)
    :mapTexture(texturePath), shader("C:\\Dev\\RunicVTT\\RunicVTT\\res\\shaders\\Basic.shader"), vb(nullptr,0), ib(0,0), renderer(rend)
{   
    viewProjectionMatrix = glm::ortho(-1.0f, 1.0f, -1.0f, 1.0f, -1.0f, 1.0f);

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
    shader.Bind();  
    shader.SetUniform1i("u_Texture", 0);
    shader.SetUniformMat4f("u_MVP", proj);
    shader.Unbind();
    va.Unbind();
    vb.Unbind();
    ib.Unbind();
}

Board::~Board()
{
}

void Board::renderToolbar()
{
}


void Board::renderMap() 
{
    shader.Bind();
    mapTexture.Bind();
    renderer.Draw(va, ib, shader); // Draw the map

    for (auto& marker : markers) {
        marker.draw(shader, viewProjectionMatrix); // Draw each marker
    }
}

void Board::handleInput()
{
}

void Board::addMarkerOnClick(ImVec2 mousePosition) 
{

}

void Board::updateCamera(float zoomLevel, const glm::vec2& panOffset) {
    // Set up orthographic projection based on zoom and pan
    float left = -1.0f * zoomLevel + panOffset.x;
    float right = 1.0f * zoomLevel + panOffset.x;
    float bottom = -1.0f * zoomLevel + panOffset.y;
    float top = 1.0f * zoomLevel + panOffset.y;

    viewProjectionMatrix = glm::ortho(left, right, bottom, top, -1.0f, 1.0f);
}


void Board::renderFogOfWar()
{
}


