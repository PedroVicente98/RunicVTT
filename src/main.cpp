
#include <GL/glew.h> 
#include <GLFW/glfw3.h>

#include "Renderer.h"
#include "Shader.h"
#include "VertexBuffer.h"
#include "IndexBuffer.h"
#include "VertexArray.h"
#include "VertexBufferLayout.h"
#include "Texture.h"

#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"

#include <filesystem>
#include <iostream>
#include <thread>
#include <mutex>
#include <condition_variable>

#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"
#include "imgui/imgui_impl_glfw.h"
#include "imgui/imgui_impl_opengl3.h"
#include "GameTable.h"
#include "Chat.h"
#include "DirectoryWindow.h"

#include "flecs.h"

#ifdef _WIN32   
// windows code goes here.  
#elif __linux__    
// linux code goes here.  
#else     
#error Platform not supported
#endif

int main(void)
{
    GLFWwindow* window;

    /* Initialize the library */
    if (!glfwInit())
        return -1;
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_RESIZABLE, GL_TRUE);
    glfwWindowHint(GLFW_MAXIMIZED, GLFW_TRUE);

    /* Create a windowed mode window and its OpenGL context */
    window = glfwCreateWindow(640, 480, "Hello World", NULL, NULL);
    if (!window)
    {
        glfwTerminate();
        return -1;
    }

    /* Make the window's context current */
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    if (glewInit() != GLEW_OK) {
        std::cout << "GLEW FAILED";
    }

    std::cout << glGetString(GL_VERSION) << std::endl;
    {//Escopo para finalizar OPenGL antes GlFW
        GLCall(glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA));
        GLCall(glEnable(GL_BLEND));

        //float positions[] = {
        //    -1.0f, -1.0f, 0.0f, 0.0f,//0
        //     1.0f, -1.0f, 1.0f, 0.0f,//1
        //     1.0f, 1.0f,  1.0f, 1.0f,//2
        //     -1.0f, 1.0f, 0.0f, 1.0f //3
        //};

        //unsigned int indices[] = {
        //    0 , 1 , 2,
        //    2 , 3 , 0
        //};


        //VertexArray va;
        //VertexBuffer vb(positions, 4/*number of vertexes*/ * 4/*floats per vertex*/ * sizeof(float));
        //VertexBufferLayout layout;
        //layout.Push<float>(2);
        //layout.Push<float>(2);
        //va.AddBuffer(vb, layout);
        //IndexBuffer ib(indices, 6);
        //glm::mat4 proj = glm::ortho(-1.0f, 1.0f, -1.0f, 1.0f, 1.0f, -1.0f);
        //Shader shader("res/shaders/Basic.shader");
        //shader.Bind();
        ////shader.SetUniform4f("u_Color", 0.8f, 0.3f, 0.8f, 1.0f);
        //shader.SetUniform1i("u_Texture", 0);
        //shader.SetUniformMat4f("u_MVP", proj);
        //Texture texture("C:\\Dev\\RunicVTT\\RunicVTT\\res\\textures\\PhandalinBattlemap2.png");
        //texture.Bind();
        //va.Unbind();
        //vb.Unbind();
        //ib.Unbind();
        //shader.Unbind();

        // REST OF INITIAL SETUP like IMGUI first layout and other configurations
        // Setup Dear ImGui context
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO(); (void)io;
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;         // Enable Docking
        io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;       // Enable Multi-Viewport / Platform Windows
        //io.ConfigViewportsNoAutoMerge = true;
        io.ConfigWindowsMoveFromTitleBarOnly = true;

        // Setup Dear ImGui style
        ImGui::StyleColorsDark();
        //ImGui::StyleColorsLight();

        // When viewports are enabled we tweak WindowRounding/WindowBg so platform windows can look identical to regular ones.
        ImGuiStyle& style = ImGui::GetStyle();
        if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
        {
            style.WindowRounding = 0.0f;
            style.Colors[ImGuiCol_WindowBg].w = 0.0f;
        }
        // Setup Platform/Renderer backends
        const char* glsl_version = "#version 330 core";
        ImGui_ImplGlfw_InitForOpenGL(window, true);
        ImGui_ImplOpenGL3_Init();

       /* Renderer renderer;
        std::string gametable_name("Main");
        GameTable gametable(gametable_name, renderer);*/

        std::filesystem::path base_path = std::filesystem::current_path();
        std::cout << "Executable Path: " << base_path << std::endl;
        std::filesystem::path map_directory_path = base_path / "res" / "textures";
        std::filesystem::path marker_directory_path = base_path / "res" / "markers";


        Chat chat;
        std::string map_directory_name = "MapDiretory";
        //std::string map_directory_path = "C:\\Dev\\RunicVTT\\RunicVTT\\res\\textures";

        std::string marker_directory_name = "MarkerDiretory";
        //std::string marker_directory_path = "C:\\Dev\\RunicVTT\\RunicVTT\\res\\markers";

        DirectoryWindow map_directory(map_directory_path.string());
        DirectoryWindow marker_directory(marker_directory_path.string());

        //DirectoryWindow map_directory(map_directory_path);
        //DirectoryWindow marker_directory(marker_directory_path);

        map_directory.startMonitoring();
        marker_directory.startMonitoring();


      /*  Texture texture1("C:\\Dev\\RunicVTT\\RunicVTT\\res\\textures\\PhandalinBattlemap2.png");
        Texture texture2("C:\\Dev\\RunicVTT\\RunicVTT\\res\\textures\\50x50-Pirate-Ship-Arrival.jpg");
        std::cout << "texture1" << texture1.GetRendererID() << " | " << texture1.m_FilePath << std::endl;
        std::cout << "texture2" << texture2.GetRendererID() << " | " << texture2.m_FilePath << std::endl;*/

        /* Loop until the user closes the window */
        while (!glfwWindowShouldClose(window))
        {
            /* Poll for and process events */
            glfwPollEvents(); 

            /* Render here */
            GLCall(glClear(GL_COLOR_BUFFER_BIT));
            // Start the Dear ImGui frame
            ImGui_ImplOpenGL3_NewFrame();
            ImGui_ImplGlfw_NewFrame();
            ImGui::NewFrame();
            ImGuiID dockspace_id = ImGui::GetID("Root");
            ImGuiViewport* viewport = ImGui::GetMainViewport();

            ImGui::BeginMainMenuBar();
            if (ImGui::BeginMenu("File")) {
                if (ImGui::MenuItem("Open", "Ctrl+O")) {
                    std::cout << "Open clicked" << std::endl;
                }
                if (ImGui::MenuItem("Save", "Ctrl+S")) {
                    std::cout << "Save clicked" << std::endl;
                }
                if (ImGui::MenuItem("Exit", "Ctrl+Q")) {
                    std::cout << "Exit clicked" << std::endl;
                }
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Edit")) {
                if (ImGui::MenuItem("Undo", "Ctrl+Z")) {
                    std::cout << "Undo clicked" << std::endl;
                }
                if (ImGui::MenuItem("Redo", "Ctrl+Y")) {
                    std::cout << "Redo clicked" << std::endl;
                }
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Help")) {
                if (ImGui::MenuItem("About")) {
                    std::cout << "About clicked" << std::endl;
                }
                ImGui::EndMenu();
            }
            ImGui::EndMainMenuBar();


            if (ImGui::DockBuilderGetNode(dockspace_id) == 0) {
                ImVec2 dockspace_size = viewport->Size;
                ImGui::DockBuilderRemoveNode(dockspace_id);
                ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_DockSpace | ImGuiDockNodeFlags_PassthruCentralNode);
                ImGui::DockBuilderSetNodeSize(dockspace_id, dockspace_size);

                ImGuiID dock_id_right = dockspace_id;
                ImGuiID dock_id_left = ImGui::DockBuilderSplitNode(dock_id_right, ImGuiDir_Left, 0.3f, NULL, &dock_id_right);
                ImGui::DockBuilderDockWindow("MapWindow", dock_id_left);
                ImGui::DockBuilderDockWindow("MapDiretory", dock_id_right);
                ImGui::DockBuilderDockWindow("MarkerDiretory", dock_id_right);
                ImGui::DockBuilderDockWindow("ChatWindow", dock_id_right);
                ImGui::DockBuilderFinish(dockspace_id);
            }

            ImGuiWindowFlags root_flags = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;
            ImGui::SetNextWindowPos(viewport->Pos, ImGuiCond_Always);
            ImGui::SetNextWindowSize(viewport->Size, ImGuiCond_Always);
            ImGui::Begin("RootWindow", nullptr, root_flags);
            ImGui::DockSpace(dockspace_id);
            ImGui::End();

            //ImGui::ShowMetricsWindow();
            ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus;
            ImGui::Begin("MapWindow", nullptr, window_flags);

            ImVec2 window_pos = ImGui::GetWindowPos();
            ImVec2 window_size = ImGui::GetWindowSize();
            int viewport_x = (int)window_pos.x; // Cast to int, ImGui uses float which might not necessarily align with pixel units
            int viewport_y = (int)(ImGui::GetIO().DisplaySize.y - window_pos.y - window_size.y); // Adjust for OpenGL's bottom-left origin
            int viewport_width = (int)window_size.x;
            int viewport_height = (int)window_size.y;
            glViewport(viewport_x, viewport_y, viewport_width, viewport_height);
            // Your map content here
            ImGui::Text("This is the central Map window.");
            //gametable.renderActiveBoard();
            ImGui::End(); 

            chat.renderChat();
            map_directory.renderDirectory(map_directory_name.c_str());
            marker_directory.renderDirectory(marker_directory_name.c_str());
 

            // Toolbar Window - Always visible on top and does not dock
            ImGui::SetNextWindowPos(ImVec2(50, 100), ImGuiCond_FirstUseEver); // Position this window to float
            ImGui::Begin("Toolbar", nullptr, ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_AlwaysAutoResize);
            ImGui::Button("Tool 1");
            ImGui::SameLine();
            ImGui::Button("Tool 2");
            //gametable.renderToolbar();
            ImGui::End();

            // Rendering
            ImGui::Render();
            ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

            if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
            {
                GLFWwindow* backup_current_context = glfwGetCurrentContext();
                ImGui::UpdatePlatformWindows();
                ImGui::RenderPlatformWindowsDefault();
                glfwMakeContextCurrent(backup_current_context);
            }
            /* Swap front and back buffers */
            glfwSwapBuffers(window);
        }
    
        map_directory.stopMonitoring();
        marker_directory.stopMonitoring();
    }

    glfwTerminate();
    return 0;
}



//#include <iostream>
//#include "Application.h"
//
//int main(void)
//{
//    Application application = Application();
//    application.create();
//    application.run();
//
//    return 0;
//}