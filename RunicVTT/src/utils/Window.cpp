#include "Window.h"
#include "imgui/imgui.h"
#include "imgui/imgui_impl_glfw.h"
#include "imgui/imgui_impl_opengl3.h"
#include "Renderer.h"
#include <filesystem>


Window::Window()
{
}

Window::Window(int window_type)
{
    switch (window_type)
    {
    case 0:
        // create a board and draw
        // this instance is now a BoardType
        break;

    case 1:
        // create a marker directory and draw
        // this instance is now a MarkerDirectoryType
        break;

    case 2:
        // create a map directory and draw
        // this instance is now a MapDirectoryType
        break;

    case 3:
        // create a chat directory and draw
        // this instance is now a ChatType
        break;

    case 4:
        // create a network log and draw
        // this instance is now a NetworkLogType
        break;
    }
}

Window::~Window()
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

void Window::draw(int window_type)
{

    switch (window_type)
    {
        case 0:
            // create a board and draw
            // this instance is now a BoardType
        break;

        case 1:
            // create a marker directory and draw
            // this instance is now a MarkerDirectoryType
        break;

        case 2:
            // create a map directory and draw
            // this instance is now a MapDirectoryType
        break;
        
        case 3:
            // create a chat directory and draw
            // this instance is now a ChatType
        break;

        case 4:
            // create a network log and draw
            // this instance is now a NetworkLogType
            break;
    }
   


}

void Window::draw_OLD()
{

    ImVec2 menuBarSize(0, 0);
    if (ImGui::BeginMainMenuBar()) {
        menuBarSize = ImGui::GetWindowSize();

        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("New")) {
                // Action for New menu item
            }
            if (ImGui::MenuItem("Open")) {
                // Action for Open menu item
            }
            ImGui::EndMenu(); 
        }
        if (ImGui::BeginMenu("Edit")) { 
            if (ImGui::MenuItem("Copy")) { 
                // Action for Copy menu item
            }
            if (ImGui::MenuItem("Paste")) { 
                // Action for Paste menu item
            }
            ImGui::EndMenu(); 
        }
        ImGui::EndMainMenuBar(); 
    }

    // First window (70% of left side)
    ImGui::SetNextWindowSize(ImVec2(ImGui::GetIO().DisplaySize.x * 0.8f, ImGui::GetIO().DisplaySize.y - menuBarSize.y));
    //ImGui::SetNextWindowPos(ImVec2(0, menuBarSize.y));
	ImGui::Begin("RunicVTT", NULL, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize);

    //board.draw_toolbar();


	// Get the position of the ImGui window
	ImVec2 windowPos = ImGui::GetWindowPos();
	int windowX = static_cast<int>(windowPos.x);
	int windowY = static_cast<int>(windowPos.y);

	// Get the size of the ImGui window
	ImVec2 windowSize = ImGui::GetWindowSize();
	int windowWidth = static_cast<int>(windowSize.x);
	int windowHeight = static_cast<int>(windowSize.y);
	// Set the OpenGL viewport to match the position and size of the ImGui window

    glViewport(windowX, windowY, windowWidth, windowHeight);
	ImGui::End();

    ImGui::SetNextWindowSize(ImVec2(ImGui::GetIO().DisplaySize.x * 0.2f, ImGui::GetIO().DisplaySize.y - menuBarSize.y));
    //ImGui::SetNextWindowPos(ImVec2(ImGui::GetIO().DisplaySize.x * 0.8f, menuBarSize.y));
    ImGui::Begin("Markers Directory", NULL, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize);
    ImGui::Text("FILE SYSTEM");
    ImGui::End();
}

void Window::clear()
{
    GLCall(glClear(GL_COLOR_BUFFER_BIT));
}
