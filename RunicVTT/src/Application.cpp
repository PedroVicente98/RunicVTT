#include "Application.h"

Application::Application()
    : window_handle(nullptr)
{
    
}

Application::~Application()
{
}

int Application::create()
{
    /* Initialize the library */
    if (!glfwInit())
        return -1;
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_RESIZABLE, GL_TRUE);
    glfwWindowHint(GLFW_MAXIMIZED, GLFW_TRUE);

    /* Create a windowed mode window and its OpenGL context */
    GLFWmonitor* primary = glfwGetPrimaryMonitor();
    const GLFWvidmode* videoMode = glfwGetVideoMode(primary);

    int monitorX, monitorY;
    glfwGetMonitorPos(primary, &monitorX, &monitorY);

    window_handle = glfwCreateWindow(640, 480, "RunicVTT", NULL, NULL);
    if (!window_handle)
    {
        glfwTerminate();
        return -1;
    }

    /* Make the window's context current */
    glfwMakeContextCurrent(window_handle);
    glfwSwapInterval(1);


    // REST OF INITIAL SETUP like IMGUI first layout and other configurations
    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;         // Enable Docking
    //io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;       // Enable Multi-Viewport / Platform Windows
    //io.ConfigViewportsNoAutoMerge = true;
    io.ConfigWindowsMoveFromTitleBarOnly = true;

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();
    //ImGui::StyleColorsLight();

    // When viewports are enabled we tweak WindowRounding/WindowBg so platform windows can look identical to regular ones.
    ImGuiStyle& style = ImGui::GetStyle();
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
    {
        //ImGui::UpdatePlatformWindows();
        //ImGui::RenderPlatformWindowsDefault();

        style.WindowRounding = 0.0f;
        style.Colors[ImGuiCol_WindowBg].w = 1.0f;
    }

    // Setup Platform/Renderer backends
    const char* glsl_version = "#version 130";
    ImGui_ImplGlfw_InitForOpenGL(window_handle, true);
    ImGui_ImplOpenGL3_Init(glsl_version);

    return 0;
}


int Application::run() {
    {//Scope to end OpenGL before GlFW
        MainWindow main_window(window_handle);

        while (!glfwWindowShouldClose(window_handle))
        {

            /* Poll for and process events */
            glfwPollEvents();
            main_window.clear();

            // Start the Dear ImGui frame
            ImGui_ImplOpenGL3_NewFrame();
            ImGui_ImplGlfw_NewFrame();
            ImGui::NewFrame();
            ImGui::DockSpaceOverViewport(NULL, ImGuiDockNodeFlags_None);
            
           
            /* Render here */

            

            main_window.draw();







            // Rendering
            ImGui::Render();
            ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

            /* Swap front and back buffers */
            glfwSwapBuffers(window_handle);
        }
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window_handle);
    glfwTerminate();
    return 0;
}