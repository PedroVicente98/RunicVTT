#include "ApplicationHandler.h"
#include "Components.h"
#include "glm/glm.hpp"


ApplicationHandler::ApplicationHandler()
    : marker_directory(std::string(), std::string()), map_directory(std::string(), std::string()), window(nullptr), game_table_manager(ecs)
{
    ecs.component<Position>();// .member<float>("x").member<float>("y");
    ecs.component<Size>();// .member<float>("width").member<float>("height");
    ecs.component<Visibility>();// .member<bool>("isVisible");
    ecs.component<Moving>();// .member<bool>("isDragging");
    ecs.component<TextureComponent>();// .member<GLuint>("textureID").member<std::string>("imagePath");
    ecs.component<Zoom>();// .member<float>("zoomLevel");
    ecs.component<Panning>();// .member<bool>("isPanning");
    ecs.component<Grid>();// .member<glm::vec2>("offset").member<glm::vec2>("scale");
    ecs.component<Board>();
    ecs.component<MarkerComponent>();
    ecs.component<FogOfWar>();
    ecs.component<GameTable>();
    ecs.component<Network>();
    ecs.component<Notes>();
    ecs.component<Tool>();

}

ApplicationHandler::~ApplicationHandler()
{
}



int ApplicationHandler::run() 
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

        /*std::filesystem::path base_path = std::filesystem::current_path();
        std::filesystem::path map_directory_path = base_path / "res" / "textures";
        std::filesystem::path marker_directory_path = base_path / "res" / "markers";
        std::string map_directory_name = "MapDiretory";
        std::string marker_directory_name = "MarkerDiretory";
        DirectoryWindow map_directory(map_directory_path.string(), map_directory_name);
        DirectoryWindow marker_directory(marker_directory_path.string(), marker_directory_name);
        map_directory.startMonitoring();
        marker_directory.startMonitoring();*/

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

            renderMainMenuBar();
            renderDockSpace();
         
            if (game_table_manager.isGameTableActive()) {
                game_table_manager.chat.renderChat();
                //ImGui::ShowMetricsWindow();
                ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoTitleBar;
                ImGui::Begin("MapWindow", nullptr, window_flags);
                ImVec2 window_pos = ImGui::GetWindowPos();
                ImVec2 window_size = ImGui::GetWindowSize();
                int viewport_x = (int)window_pos.x; // Cast to int, ImGui uses float which might not necessarily align with pixel units
                int viewport_y = (int)(ImGui::GetIO().DisplaySize.y - window_pos.y - window_size.y); // Adjust for OpenGL's bottom-left origin
                int viewport_width = (int)window_size.x;
                int viewport_height = (int)window_size.y;
                glViewport(viewport_x, viewport_y, viewport_width, viewport_height);
                //gametable.renderActiveBoard();
                ImGui::End();

            }



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

        //map_directory.stopMonitoring();
        //marker_directory.stopMonitoring();
    }

    glfwTerminate();
    return 0;

}

void ApplicationHandler::renderDockSpace() 
{

    ImGuiID dockspace_id = ImGui::GetID("Root");
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    if (ImGui::DockBuilderGetNode(dockspace_id) == 0) {
        ImGui::DockBuilderRemoveNode(dockspace_id);
        ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_DockSpace | ImGuiDockNodeFlags_PassthruCentralNode);
        ImGui::DockBuilderSetNodeSize(dockspace_id, viewport->Size);
        ImGui::DockBuilderSetNodePos(dockspace_id, viewport->Pos);

        ImGuiID dock_right_id = ImGui::DockBuilderSplitNode(dockspace_id, ImGuiDir_Right, 0.3f, nullptr, &dockspace_id);

        ImGui::DockBuilderDockWindow("MapWindow", dockspace_id);
        ImGui::DockBuilderDockWindow("MapDiretory", dock_right_id);
        ImGui::DockBuilderDockWindow("MarkerDiretory", dock_right_id);
        ImGui::DockBuilderDockWindow("ChatWindow", dock_right_id);
        ImGui::DockBuilderFinish(dockspace_id);
    }

    ImGuiWindowFlags root_flags = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;
    ImGui::SetNextWindowPos({ viewport->Pos.x, viewport->Pos.y + ImGui::GetFrameHeight() }, ImGuiCond_Always);
    ImGui::SetNextWindowSize({ viewport->Size.x, viewport->Size.y - ImGui::GetFrameHeight() }, ImGuiCond_Always);
    ImGui::Begin("RootWindow", nullptr, root_flags);
    ImGui::DockSpace(dockspace_id);
    ImGui::End();

   
}

void ApplicationHandler::renderMainMenuBar() {
    bool open_create_gametable = false;
    bool close_current_gametable = false;
    
    bool open_create_board = false;
    bool close_current_board = false;



    ImGui::BeginMainMenuBar();
    if (ImGui::BeginMenu("File")) {
        if (ImGui::MenuItem("Open", "Ctrl+O")) {
        }
        if (ImGui::MenuItem("Save", "Ctrl+S")) {
        }
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Game Table")) {
        if (ImGui::MenuItem("Create")) {
            open_create_gametable = true;
        }
        if (ImGui::MenuItem("Open", "Ctrl+Y")) {
        }
        if (ImGui::MenuItem("Close")) {
            close_current_gametable = true;
        }
        ImGui::EndMenu();
    }

    if(game_table_manager.isGameTableActive()){
        if (ImGui::BeginMenu("Board")) {
            if (ImGui::MenuItem("Create")) {
                open_create_board = true;
            }
            if (ImGui::MenuItem("Open", "Ctrl+Y")) {
            }
            ImGui::EndMenu();
        }
    }

    if (ImGui::BeginMenu("Notes")) {
        if (ImGui::MenuItem("Undo", "Ctrl+Z")) {
        }
        if (ImGui::MenuItem("Redo", "Ctrl+Y")) {
        }
        ImGui::EndMenu();
    }
    ImGui::EndMainMenuBar();

    if (open_create_gametable)
        ImGui::OpenPopup("CreateGameTable");

    if(ImGui::IsPopupOpen("CreateGameTable"))
        game_table_manager.createGameTablePopUp();
    
    if (close_current_gametable)
        ImGui::OpenPopup("CloseGameTable");

    if (ImGui::IsPopupOpen("CloseGameTable"))
        game_table_manager.closeGameTablePopUp();
    
    if (open_create_board)
        ImGui::OpenPopup("CreateBoard");

    if (ImGui::IsPopupOpen("CreateBoard"))
        game_table_manager.createBoardPopUp();


}