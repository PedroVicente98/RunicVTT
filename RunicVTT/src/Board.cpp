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
}

Board::~Board()
{
}




//void Board::draw()
//{
//    ImGuiWindowFlags flags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoCollapse;
//    ImGuiID dockspace_id = ImGui::GetID("Root");
//    ImGui::SetNextWindowDockID(dockspace_id, ImGuiCond_FirstUseEver); // Dock to the left
//    ImGui::Begin("o",NULL, flags);
//
//
//    render();
//
//
//    draw_map();
//
//    draw_toolbar(); // Draw Last to stay on top
//
//    ImGui::End();
//   
//}
//
//void Board::draw_map()
//{
//    
//   
//}
//
//void Board::draw_toolbar() 
//{
//    ImGui::BeginMenuBar();
//    ImGui::MenuItem("item.label");
//    //for (const auto& item : toolbarItems) {
//    //    ImGui::MenuItem(item.label);
//    //    ImGui::PushID(item.label);
//    //    // Begin tooltip
//    //    if (ImGui::IsItemHovered())
//    //        ImGui::SetTooltip("%s", item.tooltip);
//
//    //    // Draw button with icon and label
//    //    if (ImGui::Button((std::string(item.iconName) + "##" + item.label).c_str())) {
//    //        // Handle button click
//    //        std::cout << "CLICKED TOOL" << item.label << std::endl;
//    //    }
//    //    ImGui::SameLine();
//    //    ImGui::TextWrapped("%s", item.label);
//
//    //    ImGui::PopID();
//    //}
//
//    ImGui::EndMenuBar();
//}
//
//void Board::render()
//{
//    
//}
//
//Marker Board::create_marker(std::string file_path)
//{
//    Marker marker(file_path);
//    board_markers.push_back(marker);
//    return marker;
//}
//
//void Board::setMap(std::string file_path)
//{
//    map_path = file_path;
//    texture.SetFilePath(file_path);
//    height = texture.GetHeight();
//    width = texture.GetWidth();
//}
//
//void Board::handleMarkers()
//{
//}
//
//
//void Board::handleInput()
//{
//    handleMarkers();
//}

