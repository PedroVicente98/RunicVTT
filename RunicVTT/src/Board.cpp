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


Board::Board()
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

    VertexArray va;
    VertexBuffer vb(positions, 4/*number of vertexes*/ * 4/*floats per vertex*/ * sizeof(float));
    VertexBufferLayout layout;

    layout.Push<float>(2);
    layout.Push<float>(2);
    va.AddBuffer(vb, layout);

    IndexBuffer ib(indices, 6);

    glm::mat4 proj = glm::ortho(-1.0f, 1.0f, -1.0f, 1.0f, 1.0f, -1.0f);

    Shader shader("res/shaders/Basic.shader");
    shader.Bind();

    shader.SetUniform1i("u_Texture", 0);
    shader.SetUniformMat4f("u_MVP", proj);

    //Texture texture("res/texture/PhandalinBattlemap2.png");
    Texture texture("C:\\Dev\\TCC\\RunicVTT\\RunicVTT\\res\\textures\\PhandalinBattlemap2.png");
    //Texture texture("\\res\\texture\\PhandalinBattlemap2.png");
    texture.Bind();

    va.Unbind();
    vb.Unbind();
    ib.Unbind();
    shader.Unbind();

    Renderer renderer;

    shader.Bind();
    renderer.Draw(va, ib, shader);


}

Board::~Board()
{
}

void Board::draw_ui()
{
    //IMGUI UI FOR BOARD FUNCIONALITIES

}

void Board::draw_map()
{
    //MAP IMAGE WITH MOVE AND ZOOM APPLIED, and MARKERS MOVEMENT
}

void Board::draw_toolbar() {

    ImGui::Begin("Toolbar", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);
    for (const auto& item : toolbarItems) {
        ImGui::PushID(item.label);

        // Begin tooltip
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("%s", item.tooltip);

        // Draw button with icon and label
        if (ImGui::Button((std::string(item.iconName) + "##" + item.label).c_str())) {
            // Handle button click
        }
        ImGui::SameLine();
        ImGui::TextWrapped("%s", item.label);

        ImGui::PopID();
    }

    ImGui::End();
}

