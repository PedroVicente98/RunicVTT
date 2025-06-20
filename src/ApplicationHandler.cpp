#include "ApplicationHandler.h"
#include "Components.h"

ApplicationHandler::ApplicationHandler(GLFWwindow* window, std::shared_ptr<DirectoryWindow> map_directory, std::shared_ptr<DirectoryWindow> marker_directoryry)
    : marker_directory(marker_directoryry), map_directory(map_directory), game_table_manager(ecs, map_directory, marker_directoryry), window(window), dockspace_id(0)
{
    ecs.component<Position>();// .member<float>("x").member<float>("y");
    ecs.component<Size>();// .member<float>("width").member<float>("height");
    ecs.component<Visibility>();// .member<bool>("isVisible");
    ecs.component<Moving>();// .member<bool>("isDragging");
    ecs.component<TextureComponent>();// .member<GLuint>("textureID").member<std::string>("imagePath");
    //ecs.component<Zoom>();// .member<float>("zoomLevel");
    ecs.component<Panning>();// .member<bool>("isPanning");
    ecs.component<Grid>();// .member<glm::vec2>("offset").member<glm::vec2>("scale");
    ecs.component<Board>();
    ecs.component<MarkerComponent>();
    ecs.component<FogOfWar>();
    ecs.component<GameTable>();
    ecs.component<Network>();
    ecs.component<Notes>();
    ecs.component<Identifier>();
    ecs.component<NoteComponent>();
    ecs.component<PeerInfo>();
    //ecs.component<ToolComponent>();
   
}

ApplicationHandler::~ApplicationHandler()
{
}

int ApplicationHandler::run() 
{
    /* Make the window's context current */
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    game_table_manager.setInputCallbacks(window);
    glfwSetWindowUserPointer(window, &game_table_manager);
    int minWidth = 1280; // AJUSTAR TAMANHOS, VERIFICAR TAMANHO MINIMO QUE NÂO QUEBRA O LAYOUT
    int minHeight = 960;
    glfwSetWindowSizeLimits(window, minWidth, minHeight, GLFW_DONT_CARE, GLFW_DONT_CARE);

    // Force the window to maximize
    glfwMaximizeWindow(window);

    std::cout << glGetString(GL_VERSION) << std::endl;
    {//Escopo para finalizar OPenGL antes GlFW
        GLCall(glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA));
        GLCall(glEnable(GL_BLEND));
        GLCall(glDisable(GL_DEPTH_TEST));

        //float positions[] = {
        //     -1.0f, -1.0f, 0.0f, 0.0f,//0
        //      1.0f, -1.0f, 1.0f, 0.0f,//1
        //      1.0f, 1.0f,  1.0f, 1.0f,//2
        //      -1.0f, 1.0f, 0.0f, 1.0f //3
        //};
        float positions[] = {
            -0.5f, -0.5f, 0.0f, 0.0f, // bottom-left corner
             0.5f, -0.5f, 1.0f, 0.0f, // bottom-right corner
             0.5f,  0.5f, 1.0f, 1.0f, // top-right corner
            -0.5f,  0.5f, 0.0f, 1.0f  // top-left corner
        };

        unsigned int indices[] = {
            0 , 1 , 2,
            2 , 3 , 0
        };


        VertexArray va;
        VertexBuffer vb(positions, 4/*number of vertexes*/ * 4/*floats per vertex*/ * sizeof(float));
        VertexBufferLayout layout;

        layout.Push<float>(2);
        layout.Push<float>(2);
        va.AddBuffer(vb, layout);

        IndexBuffer ib(indices, 6);

        glm::mat4 proj = glm::ortho(-1.0f, 1.0f, -1.0f, 1.0f, 1.0f, -1.0f);

        std::filesystem::path shader_path = PathManager::getShaderPath() / "Basic.shader";
        Shader shader(shader_path.string());
        shader.Bind();
        shader.SetUniform1i("u_Texture", 0);
        shader.SetUniformMat4f("u_MVP", proj);
        shader.SetUniform1i("u_UseTexture", 1);
        shader.SetUniform1f("u_Alpha", 0.5f);

        va.Unbind();
        vb.Unbind();
        ib.Unbind();
        shader.Unbind();

        Renderer renderer;

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

            //game_table_manager.processSentMessages();
            //game_table_manager.processReceivedMessages();
            renderMainMenuBar();
            renderDockSpace();
            renderActiveGametable(va, ib, shader, renderer);

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
    dockspace_id = ImGui::GetID("Root");
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    if (ImGui::DockBuilderGetNode(dockspace_id) == 0) {
        ImGui::DockBuilderRemoveNode(dockspace_id);
        ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_DockSpace | ImGuiDockNodeFlags_PassthruCentralNode);
        ImGui::DockBuilderSetNodeSize(dockspace_id, viewport->Size);
        ImGui::DockBuilderSetNodePos(dockspace_id, viewport->Pos);

        ImGuiID dock_right_id = ImGui::DockBuilderSplitNode(dockspace_id, ImGuiDir_Right, 0.3f, nullptr, &dockspace_id);

        ImGui::DockBuilderDockWindow("MapWindow", dockspace_id);
        ImGui::DockBuilderDockWindow("ChatWindow", dock_right_id);
        //ImGui::DockBuilderDockWindow("MapDiretory", dock_right_id);
        ImGui::DockBuilderDockWindow("MarkerDiretory", dock_right_id);
        ImGui::DockBuilderFinish(dockspace_id);
    }

    ImGuiWindowFlags root_flags = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;
    ImGui::SetNextWindowPos({ viewport->Pos.x, viewport->Pos.y + ImGui::GetFrameHeight() }, ImGuiCond_Always);
    ImGui::SetNextWindowSize({ viewport->Size.x, viewport->Size.y - ImGui::GetFrameHeight() }, ImGuiCond_Always);
    ImGui::Begin("RootWindow", nullptr, root_flags);
    ImGui::DockSpace(dockspace_id);
    ImGui::End();
}

void ApplicationHandler::renderActiveGametable(VertexArray& va, IndexBuffer& ib, Shader& shader, Renderer& renderer) {


    if (game_table_manager.isGameTableActive() /*|| game_table_manager.isConnected()*/) {

        if (game_table_manager.board_manager.isEditWindowOpen()) {
            game_table_manager.board_manager.renderEditWindow();
        }
        else {
            game_table_manager.board_manager.setShowEditWindow(false);
        }
        game_table_manager.chat.renderChat();

        ImGui::ShowMetricsWindow();
        
        ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus /*| ImGuiWindowFlags_NoTitleBar*/ | ImGuiWindowFlags_NoDecoration;
        
        ImGui::SetNextWindowDockID(dockspace_id, ImGuiCond_Once);
        ImGui::Begin("MapWindow", nullptr, window_flags);
        if (game_table_manager.isBoardActive()) {

            ImVec2 window_size = ImGui::GetWindowSize();
            ImVec2 window_pos = ImGui::GetWindowPos();
            game_table_manager.setCameraWindowSizePos(glm::vec2(window_size.x, window_size.y), glm::vec2(window_pos.x, window_pos.y));

            ImGui::SetCursorScreenPos(window_pos);
            ImGui::InvisibleButton("##MapDropArea", window_size);
            // Handle the drop payload
            if (ImGui::BeginDragDropTarget()) {
                if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("MARKER_IMAGE")) {
                    
                    const DirectoryWindow::ImageData* markerImage = (const DirectoryWindow::ImageData*)payload->Data;

                    ImVec2 mouse_position = ImGui::GetMousePos();
   
                    glm::vec2 relative_screen_position = { mouse_position.x - window_pos.x, mouse_position.y - window_pos.y };

                    glm::vec2 world_position = game_table_manager.board_manager.screenToWorldPosition(relative_screen_position);
                    game_table_manager.board_manager.createMarker(markerImage->filename, markerImage->textureID, world_position, markerImage->size);

                }
                ImGui::EndDragDropTarget();
            }
        }


        ImVec2 window_pos = ImGui::GetWindowPos();
        ImVec2 window_size = ImGui::GetWindowSize();
        int viewport_x = (int)window_pos.x; // Cast to int, ImGui uses float which might not necessarily align with pixel units
        int viewport_y = (int)(ImGui::GetIO().DisplaySize.y - window_pos.y - window_size.y); // Adjust for OpenGL's bottom-left origin
        int viewport_width = (int)window_size.x;
        int viewport_height = (int)window_size.y;
        GLCall(glViewport(viewport_x, viewport_y, viewport_width, viewport_height));

        game_table_manager.render(va, ib, shader, renderer);

        ImGui::End();
    }

}

void ApplicationHandler::renderMainMenuBar() {
    bool open_create_gametable = false;
    bool close_current_gametable = false;
    bool connect_to_gametable = false;
    bool load_active_gametable = false;
    
    bool open_create_board = false;
    bool close_current_board = false;

    bool save_active_board = false;
    bool load_active_board = false;


    bool open_network_connection = false;
    bool open_network_info = false;
    bool close_network_connection = false;

    ImGui::BeginMainMenuBar();
    if (ImGui::BeginMenu("Game Table")) {
        if (ImGui::MenuItem("Create")) {
            open_create_gametable = true;
        }
        if (ImGui::MenuItem("Connect")) {
            connect_to_gametable = true;
        }
        if (game_table_manager.isGameTableActive()) {
            if (ImGui::MenuItem("Save"))
            {
                game_table_manager.saveGameTable();
            }
        }
        
        if (ImGui::MenuItem("Open")) 
        {
            load_active_gametable = true;
        }
        if (ImGui::MenuItem("Close")) {
            close_current_gametable = true;
        }
        ImGui::EndMenu();
    }

    if(game_table_manager.isGameTableActive()){

        if (ImGui::BeginMenu("Network")) {
            bool is_connection_active = false/*game_table_manager.isConnectionActive()*/;
            //if(!game_table_manager.isConnectionActive()){
            //    if (ImGui::MenuItem("Open Connection")) { //ADD LATER TO REOPEN A CONNECTION, NEED TO SAVE THE PORT
            //        open_network_connection = true;
            //    } 
            //}
            
            if (ImGui::MenuItem("Connection Info")) {
                open_network_info = true;
            }

            if (ImGui::MenuItem("Close Connection", 0, false, is_connection_active)) {
                close_network_connection = true;
            }
            ImGui::EndMenu();
        }
        

        if (ImGui::BeginMenu("Board")) {
            if (ImGui::MenuItem("Create")) {
                open_create_board = true;
            }
            if (game_table_manager.board_manager.isBoardActive()) {
                if (ImGui::MenuItem("Save")) {
                    //save_active_board = true;
                    auto board_folder_path = PathManager::getGameTablesPath() / game_table_manager.game_table_name / "Boards";
                    game_table_manager.board_manager.saveActiveBoard(board_folder_path);
                }

                if (ImGui::MenuItem("Close")) {
                    close_current_board = true;
                }
            }
            if (ImGui::MenuItem("Open")) {
                load_active_board = true;
            }
         
            ImGui::EndMenu();
        }
    }

 /*   if (ImGui::BeginMenu("Notes")) {
        if (ImGui::MenuItem("Undo", "Ctrl+Z")) {
        }
        if (ImGui::MenuItem("Redo", "Ctrl+Y")) {
        }
        ImGui::EndMenu();
    }*/

    ImGui::EndMainMenuBar();


    if (load_active_gametable)
        ImGui::OpenPopup("LoadGameTable");

    if (ImGui::IsPopupOpen("LoadGameTable"))
        game_table_manager.loadGameTablePopUp();

    //if (save_active_board)
    //    ImGui::OpenPopup("SaveBoard");

    //if (ImGui::IsPopupOpen("SaveBoard"))
    //    game_table_manager.saveBoardPopUp();

    if (load_active_board)
        ImGui::OpenPopup("LoadBoard");

    if (ImGui::IsPopupOpen("LoadBoard"))
        game_table_manager.loadBoardPopUp();

    if (close_network_connection)
        ImGui::OpenPopup("CloseNetwork");

    if(ImGui::IsPopupOpen("CloseNetwork"))
        game_table_manager.closeNetworkPopUp();

    if (open_network_info)
        ImGui::OpenPopup("NetworkInfo");

    if(ImGui::IsPopupOpen("NetworkInfo"))
        game_table_manager.openNetworkInfoPopUp();
     
     if (open_network_connection)
        ImGui::OpenPopup("CreateNetwork");

    if(ImGui::IsPopupOpen("CreateNetwork"))
        game_table_manager.createNetworkPopUp();

    if (connect_to_gametable)
        ImGui::OpenPopup("ConnectToGameTable");

    if(ImGui::IsPopupOpen("ConnectToGameTable"))
        game_table_manager.connectToGameTablePopUp();

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
    
    if (close_current_board)
        ImGui::OpenPopup("CloseBoard");

    if (ImGui::IsPopupOpen("CloseBoard"))
        game_table_manager.closeBoardPopUp();


}
