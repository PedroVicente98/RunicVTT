#include <GL/glew.h> 
#include <GLFW/glfw3.h>
#include "BoardManager.h"

#include "Renderer.h"
#include "Shader.h"
#include "VertexBuffer.h"
#include "IndexBuffer.h"
#include "VertexArray.h"
#include "VertexBufferLayout.h"
#include "Texture.h"

#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"

BoardManager::BoardManager()
{   
  
}

BoardManager::~BoardManager()
{
}

bool BoardManager::isBoardActive()
{
    return active_board.is_valid();
}

void BoardManager::renderToolbar()
{
}

void BoardManager::renderMap() 
{
}

void BoardManager::handleInput()
{
}

void BoardManager::addMarkerOnClick() 
{

}

void BoardManager::renderFogOfWar()
{
}




//void BoardManager::updateCamera(float zoomLevel, const glm::vec2& panOffset) {
//    // Set up orthographic projection based on zoom and pan
//    float left = -1.0f * zoomLevel + panOffset.x;
//    float right = 1.0f * zoomLevel + panOffset.x;
//    float bottom = -1.0f * zoomLevel + panOffset.y;
//    float top = 1.0f * zoomLevel + panOffset.y;
//
//    viewProjectionMatrix = glm::ortho(left, right, bottom, top, -1.0f, 1.0f);
//}
/*
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


*/