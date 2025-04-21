
#include "ApplicationHandler.h"

GLFWwindow* initializeOpenGLContext() {
    if (!glfwInit()) {
        std::cerr << "Falha ao inicializar o GLFW!" << std::endl;
        return nullptr;
    }

    // Cria a janela e inicializa o contexto OpenGL
    GLFWwindow* window = glfwCreateWindow(1280, 720, "Runic VTT", nullptr, nullptr);
    if (!window) {
        std::cerr << "Falha ao criar a janela GLFW!" << std::endl;
        glfwTerminate();
        return nullptr;
    }

    // Faz o contexto OpenGL atual para a janela
    glfwMakeContextCurrent(window);

    // Inicializa o GLEW (opcional, se estiver usando)
    if (glewInit() != GLEW_OK) {
        std::cerr << "Falha ao inicializar o GLEW!" << std::endl;
        return nullptr;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
    glfwWindowHint(GLFW_MAXIMIZED, GLFW_TRUE);

    // Retorna o ponteiro da janela
    return window;
}


static std::string setupRootDirectory() {
    auto rootDirectory = std::filesystem::current_path();
    if (rootDirectory.filename() == "RunicVTT") {
        auto games_tables_directory = rootDirectory / "GameTables";
        if (!fs::exists(games_tables_directory)) {
            if (fs::create_directory(games_tables_directory)) {
                std::cout << "GameTables directory created successfully: " << games_tables_directory << std::endl;
            }
        }

        auto maps_directory = rootDirectory / "Maps";
        if (!fs::exists(maps_directory)) {
            if (fs::create_directory(maps_directory)) {
                std::cout << "Maps directory created successfully: " << maps_directory << std::endl;
            }
        }

        auto markers_directory = rootDirectory / "Markers";
        if (!fs::exists(markers_directory)) {
            if (fs::create_directory(markers_directory)) {
                std::cout << "Markers directory created successfully: " << markers_directory << std::endl;
            }
        }
    }
    return rootDirectory.string();

}

// Função para definir o ícone da janela
void setWindowIcon(GLFWwindow* window, const char* iconPath) {
    // Carrega a imagem do ícone usando stb_image
    int width, height, channels;
    unsigned char* image = stbi_load(iconPath, &width, &height, &channels, 4);  // Força 4 canais (RGBA)
    if (!image) {
        std::cerr << "Erro: Não foi possível carregar o ícone: " << iconPath << std::endl;
        return;
    }

    // Cria o objeto GLFWimage e configura o ícone
    GLFWimage icon;
    icon.width = width;
    icon.height = height;
    icon.pixels = image;
    glfwSetWindowIcon(window, 1, &icon);  // Define o ícone

    // Libera a memória usada pela imagem
    stbi_image_free(image);
}

int main() {
//
//#ifndef _DEBUG
//    // Hide the console window in Release mode
//    ::ShowWindow(::GetConsoleWindow(), SW_HIDE);
//#endif
    // Inicializa o contexto OpenGL
    GLFWwindow* window = initializeOpenGLContext();
    if (!window) {
        return -1;  // Falha na inicialização
    }

    auto rootDirectory = setupRootDirectory();
    auto iconPath = rootDirectory + "/res/RunicVTTIcon.png";
    setWindowIcon(window, iconPath.c_str());
    ApplicationHandler app(window, rootDirectory);
    app.run();  // Executa o loop principal da aplicação

    glfwTerminate();  // Limpa os recursos do GLFW quando terminar
    return 0;
}


//
//int main(void)
//{
//    GLFWwindow* window;
//    /* Initialize the library */
//    if (!glfwInit())
//        return -1;
//    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
//    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
//    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
//    glfwWindowHint(GLFW_RESIZABLE, GL_TRUE);
//    glfwWindowHint(GLFW_MAXIMIZED, GLFW_TRUE);
//
//    /* Create a windowed mode window and its OpenGL context */
//    window = glfwCreateWindow(640, 480, "Hello World", NULL, NULL);
//    if (!window)
//    {
//        glfwTerminate();
//        return -1;
//    }
//
//    /* Make the window's context current */
//    glfwMakeContextCurrent(window);
//    glfwSwapInterval(1);
//
//    if (glewInit() != GLEW_OK) {
//        std::cout << "GLEW FAILED";
//    }
//
//    std::cout << glGetString(GL_VERSION) << std::endl;
//    {//Escopo para finalizar OPenGL antes GlFW
//        GLCall(glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA));
//        GLCall(glEnable(GL_BLEND));
//
//       
//        IMGUI_CHECKVERSION();
//        ImGui::CreateContext();
//        ImGuiIO& io = ImGui::GetIO(); (void)io;
//        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
//        io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls
//        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;         // Enable Docking
//        io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;       // Enable Multi-Viewport / Platform Windows
//        //io.ConfigViewportsNoAutoMerge = true;
//        io.ConfigWindowsMoveFromTitleBarOnly = true;
//
//        // Setup Dear ImGui style
//        ImGui::StyleColorsDark();
//        //ImGui::StyleColorsLight();
//
//        // When viewports are enabled we tweak WindowRounding/WindowBg so platform windows can look identical to regular ones.
//        ImGuiStyle& style = ImGui::GetStyle();
//        if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
//        {
//            style.WindowRounding = 0.0f;
//            style.Colors[ImGuiCol_WindowBg].w = 0.0f;
//        }
//        // Setup Platform/Renderer backends
//        const char* glsl_version = "#version 330 core";
//        ImGui_ImplGlfw_InitForOpenGL(window, true);
//        ImGui_ImplOpenGL3_Init();
//
//        std::filesystem::path base_path = std::filesystem::current_path();
//        std::filesystem::path map_directory_path = base_path / "res" / "textures";
//        std::filesystem::path marker_directory_path = base_path / "res" / "markers";
//        Chat chat;
//        std::string map_directory_name = "MapDiretory";
//        std::string marker_directory_name = "MarkerDiretory";
//
//        DirectoryWindow map_directory(map_directory_path.string(), map_directory_name);
//        DirectoryWindow marker_directory(marker_directory_path.string(), marker_directory_name);
//
//        map_directory.startMonitoring();
//        marker_directory.startMonitoring();
//
//        /* Loop until the user closes the window */
//        while (!glfwWindowShouldClose(window))
//        {
//            /* Poll for and process events */
//            glfwPollEvents(); 
//            /* Render here */
//            GLCall(glClear(GL_COLOR_BUFFER_BIT));
//            // Start the Dear ImGui frame
//            ImGui_ImplOpenGL3_NewFrame();
//            ImGui_ImplGlfw_NewFrame();
//            ImGui::NewFrame();
//            //ImGuiID dockspace_id = ImGui::GetID("Root");
//            //ImGuiViewport* viewport = ImGui::GetMainViewport();
//            //ImVec2 mainMenuBarSize;
//            //ImGui::BeginMainMenuBar();
//            //mainMenuBarSize = ImGui::GetWindowSize();
//            //if (ImGui::BeginMenu("File")) {
//            //    if (ImGui::MenuItem("Open", "Ctrl+O")) {
//            //        std::cout << "Open clicked" << std::endl;
//            //    }
//            //    if (ImGui::MenuItem("Save", "Ctrl+S")) {
//            //        std::cout << "Save clicked" << std::endl;
//            //    }
//            //    if (ImGui::MenuItem("Exit", "Ctrl+Q")) {
//            //        std::cout << "Exit clicked" << std::endl;
//            //    }
//            //    ImGui::EndMenu();
//            //}
//            //if (ImGui::BeginMenu("Edit")) {
//            //    if (ImGui::MenuItem("Undo", "Ctrl+Z")) {
//            //        std::cout << "Undo clicked" << std::endl;
//            //    }
//            //    if (ImGui::MenuItem("Redo", "Ctrl+Y")) {
//            //        std::cout << "Redo clicked" << std::endl;
//            //    }
//            //    ImGui::EndMenu();
//            //}
//            //if (ImGui::BeginMenu("Help")) {
//            //    if (ImGui::MenuItem("About")) {
//            //        std::cout << "About clicked" << std::endl;
//            //    }
//            //    ImGui::EndMenu();
//            //}
//            //ImGui::EndMainMenuBar();
//
//
//            //ImVec2 dockspace_size = { viewport->Size.x, viewport->Size.y - mainMenuBarSize.y };
//            //if (ImGui::DockBuilderGetNode(dockspace_id) == 0) {
//            //    ImGui::DockBuilderRemoveNode(dockspace_id);
//            //    ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_DockSpace | ImGuiDockNodeFlags_PassthruCentralNode);
//            //    ImGui::DockBuilderSetNodeSize(dockspace_id, dockspace_size);
//
//            //    ImGuiID dock_id_right = dockspace_id;
//            //    ImGuiID dock_id_left = ImGui::DockBuilderSplitNode(dock_id_right, ImGuiDir_Left, 0.3f, NULL, &dock_id_right);
//            //    ImGui::DockBuilderDockWindow("MapWindow", dock_id_left);
//            //    ImGui::DockBuilderDockWindow("MapDiretory", dock_id_right);
//            //    ImGui::DockBuilderDockWindow("MarkerDiretory", dock_id_right);
//            //    ImGui::DockBuilderDockWindow("ChatWindow", dock_id_right);
//            //    ImGui::DockBuilderFinish(dockspace_id);
//            //}
//
//            //ImGuiWindowFlags root_flags = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;
//            //ImGui::SetNextWindowPos(viewport->Pos, ImGuiCond_Always);
//            //ImGui::SetNextWindowSize(dockspace_size, ImGuiCond_Always);
//            //ImGui::Begin("RootWindow", nullptr, root_flags);
//            //ImGui::DockSpace(dockspace_id);
//            //ImGui::End();
//
//            ////ImGui::ShowMetricsWindow();
//            //ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus;
//            //ImGui::Begin("MapWindow", nullptr, window_flags);
//
//            //ImVec2 window_pos = ImGui::GetWindowPos();
//            //ImVec2 window_size = ImGui::GetWindowSize();
//            //int viewport_x = (int)window_pos.x; // Cast to int, ImGui uses float which might not necessarily align with pixel units
//            //int viewport_y = (int)(ImGui::GetIO().DisplaySize.y - window_pos.y - window_size.y); // Adjust for OpenGL's bottom-left origin
//            //int viewport_width = (int)window_size.x;
//            //int viewport_height = (int)window_size.y;
//            //glViewport(viewport_x, viewport_y, viewport_width, viewport_height);
//            //// Your map content here
//            //ImGui::Text("This is the central Map window.");
//            ////gametable.renderActiveBoard();
//            //ImGui::End(); 
//
//            //chat.renderChat();
//            //map_directory.renderDirectory();
//            //marker_directory.renderDirectory();
// 
//
//            //// Toolbar Window - Always visible on top and does not dock
//            //ImGui::SetNextWindowPos(ImVec2(50, 100), ImGuiCond_FirstUseEver); // Position this window to float
//            //ImGui::Begin("Toolbar", nullptr, ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_AlwaysAutoResize);
//            //ImGui::Button("Tool 1");
//            //ImGui::SameLine();
//            //ImGui::Button("Tool 2");
//            ////gametable.renderToolbar();
//            //ImGui::End();
//
//            // Rendering
//            ImGui::Render();
//            ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
//
//            if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
//            {
//                GLFWwindow* backup_current_context = glfwGetCurrentContext();
//                ImGui::UpdatePlatformWindows();
//                ImGui::RenderPlatformWindowsDefault();
//                glfwMakeContextCurrent(backup_current_context);
//            }
//            /* Swap front and back buffers */
//            glfwSwapBuffers(window);
//        }
//    
//        map_directory.stopMonitoring();
//        marker_directory.stopMonitoring();
//    }
//
//    glfwTerminate();
//    return 0;
//}
//
