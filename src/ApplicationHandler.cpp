#include "ApplicationHandler.h"
#include "Components.h"
#include "NetworkUtilities.h"
#include "DebugConsole.h"
#include "Logger.h"
#include "DebugActions.h"
#include "FirewallUtils.h"
#include "AssetIO.h"
#include <chrono>
#include <string>

ApplicationHandler::ApplicationHandler(GLFWwindow* window, std::shared_ptr<DirectoryWindow> map_directory, std::shared_ptr<DirectoryWindow> marker_directoryry) :
    marker_directory(marker_directoryry), map_directory(map_directory), game_table_manager(std::make_shared<GameTableManager>(ecs, map_directory, marker_directoryry)), window(window), g_dockspace_initialized(false), map_fbo(std::make_shared<MapFBO>())
{
    ImGuiToaster::Config cfg;
    this->toaster_ = std::make_shared<ImGuiToaster>(cfg);
    game_table_manager->setup();
    game_table_manager->setToaster(toaster_);

    ecs.component<Position>();         // .member<float>("x").member<float>("y");
    ecs.component<Size>();             // .member<float>("width").member<float>("height");
    ecs.component<Visibility>();       // .member<bool>("isVisible");
    ecs.component<Moving>();           // .member<bool>("isDragging");
    ecs.component<TextureComponent>(); // .member<GLuint>("textureID").member<std::string>("imagePath");
    //ecs.component<Zoom>();// .member<float>("zoomLevel");
    ecs.component<Panning>(); // .member<bool>("isPanning");
    ecs.component<Grid>();    // .member<glm::vec2>("offset").member<glm::vec2>("scale");
    ecs.component<Board>();
    ecs.component<MarkerComponent>();
    ecs.component<FogOfWar>();
    ecs.component<GameTable>();
    //ecs.component<Network>();
    ecs.component<Notes>();
    ecs.component<Identifier>();
    ecs.component<NoteComponent>();
    //ecs.component<PeerInfo>();
    //ecs.component<ToolComponent>();
    DebugActions::RegisterToasterToggles(toaster_);
    DebugActions::RegisterAllDefaultToggles();
}

ApplicationHandler::~ApplicationHandler()
{
    DeleteMapFBO();
}

void ApplicationHandler::CreateMapFBO(int width, int height)
{
    if (map_fbo->fboID != 0)
    { // If FBO already exists, delete it first
        DeleteMapFBO();
    }

    if (width <= 0 || height <= 0)
    {
        std::cerr << "Warning: Attempted to create FBO with invalid dimensions: " << width << "x" << height << std::endl;
        // Keep FBO as 0,0,0 if dimensions are invalid to avoid errors
        map_fbo->width = 0;
        map_fbo->height = 0;
        return;
    }

    map_fbo->width = width;
    map_fbo->height = height;

    GLCall(glGenFramebuffers(1, &map_fbo->fboID));
    GLCall(glBindFramebuffer(GL_FRAMEBUFFER, map_fbo->fboID));

    // Create a color attachment texture
    GLCall(glGenTextures(1, &map_fbo->textureID));
    GLCall(glBindTexture(GL_TEXTURE_2D, map_fbo->textureID));
    GLCall(glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr));
    GLCall(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR));
    GLCall(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR));
    GLCall(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE)); // Important for non-power-of-2 textures
    GLCall(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE));
    GLCall(glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, map_fbo->textureID, 0));

    // Create a renderbuffer object for depth and stencil attachment
    // This is crucial for proper 3D rendering (correct Z-buffer, stencil for fog, etc.)
    GLCall(glGenRenderbuffers(1, &map_fbo->rboID));
    GLCall(glBindRenderbuffer(GL_RENDERBUFFER, map_fbo->rboID));
    GLCall(glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height));
    GLCall(glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, map_fbo->rboID));

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
    {
        std::cerr << "ERROR::FRAMEBUFFER:: Framebuffer is not complete! Status: " << glCheckFramebufferStatus(GL_FRAMEBUFFER) << std::endl;
    }

    GLCall(glBindFramebuffer(GL_FRAMEBUFFER, 0)); // Unbind FBO, return to default framebuffer
}

void ApplicationHandler::ResizeMapFBO(int newWidth, int newHeight)
{
    if (map_fbo->width == newWidth && map_fbo->height == newHeight)
    {
        return; // No resize needed
    }

    if (newWidth <= 0 && newHeight <= 0)
    {
        // Prevent creating FBO with invalid sizes, might occur when window is minimized
        if (map_fbo->fboID != 0)
            DeleteMapFBO(); // Clean up if it exists
        return;
    }
    // Only recreate if dimensions actually changed
    if (map_fbo->width == newWidth && map_fbo->height == newHeight)
    {
        return;
    }
    CreateMapFBO(newWidth, newHeight); // Recreate FBO with new dimensions
}

void ApplicationHandler::DeleteMapFBO()
{
    if (map_fbo->fboID != 0)
    {
        GLCall(glDeleteFramebuffers(1, &map_fbo->fboID));
        map_fbo->fboID = 0;
    }
    if (map_fbo->textureID != 0)
    {
        GLCall(glDeleteTextures(1, &map_fbo->textureID));
        map_fbo->textureID = 0;
    }
    if (map_fbo->rboID != 0)
    {
        GLCall(glDeleteRenderbuffers(1, &map_fbo->rboID));
        map_fbo->rboID = 0;
    }
    map_fbo->width = 0;
    map_fbo->height = 0;
}

void ApplicationHandler::TickAutoSave()
{
    using clock = std::chrono::steady_clock;
    static clock::time_point s_last = clock::now();

    // Early-out if not time yet (every 60s)
    constexpr auto kInterval = std::chrono::minutes(1);
    const auto now = clock::now();
    if (now - s_last < kInterval)
        return;
    s_last = now;
    if (game_table_manager->network_manager)
    {
        if (game_table_manager->network_manager->getPeerRole() == Role::GAMEMASTER)
        {

            if (!game_table_manager)
            {
                if (toaster_)
                    toaster_->Push(ImGuiToaster::Level::Warning, "Autosave skipped: GameTableManager missing", 4.0f);
                return;
            }

            bool ok = true;
            std::string err;

            // 1) Save active GameTable (your method name as provided)
            try
            {
                auto gametable_entity = game_table_manager->getActiveGameTableEntity();
                if (gametable_entity.is_valid())
                {
                    game_table_manager->saveGameTable(); // (or saveActiveGametable() if that's your actual name)
                }
            }
            catch (const std::exception& e)
            {
                ok = false;
                err += std::string("GameTable: ") + e.what();
            }
            catch (...)
            {
                ok = false;
                err += "GameTable: unknown error";
            }

            // 2) Save active Board
            try
            {
                if (game_table_manager->board_manager)
                {
                    auto gametable_entity = game_table_manager->getActiveGameTableEntity();
                    if (gametable_entity.is_valid())
                    {
                        auto game_table_name = gametable_entity.get<GameTable>()->gameTableName;
                        auto board_dir_path = PathManager::getBoardsPath(game_table_name);
                        game_table_manager->board_manager->saveActiveBoard(board_dir_path);
                    }
                }
                else
                {
                    ok = false;
                    if (!err.empty())
                        err += " | ";
                    err += "BoardManager missing";
                }
            }
            catch (const std::exception& e)
            {
                ok = false;
                if (!err.empty())
                    err += " | ";
                err += std::string("Board: ") + e.what();
            }
            catch (...)
            {
                ok = false;
                if (!err.empty())
                    err += " | ";
                err += "Board: unknown error";
            }

            // Toast result
            if (toaster_)
            {
                if (ok)
                    toaster_->Push(ImGuiToaster::Level::Good, "Autosave complete: GameTable & Board", 5.0f);
                else
                    toaster_->Push(ImGuiToaster::Level::Error, std::string("Autosave failed: ") + err, 6.0f);
            }
        }
    }
    else
    {
        return;
    }

    // Sanity checks
    // 3) Notes (not implemented) — left as-is
    // notes_manager->saveOpenNotes();
}

int ApplicationHandler::run()
{
    /* Make the window's context current */
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    glfwSetWindowUserPointer(window, &game_table_manager);
    int minWidth = 1280; // AJUSTAR TAMANHOS, VERIFICAR TAMANHO MINIMO QUE NÃ‚O QUEBRA O LAYOUT
    int minHeight = 960;
    glfwSetWindowSizeLimits(window, minWidth, minHeight, GLFW_DONT_CARE, GLFW_DONT_CARE);

    // Force the window to maximize
    glfwMaximizeWindow(window);

    std::cout << glGetString(GL_VERSION) << std::endl;
    { //Escopo para finalizar OPenGL antes GlFW
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
            0.5f, 0.5f, 1.0f, 1.0f,   // top-right corner
            0.5f, -0.5f, 1.0f, 0.0f,  // bottom-right corner
            -0.5f, -0.5f, 0.0f, 0.0f, // bottom-left corner
            -0.5f, 0.5f, 0.0f, 1.0f   // top-left corner
        };

        unsigned int indices[] = {
            0, 1, 2,
            2, 3, 0};

        VertexArray va;
        VertexBuffer vb(positions, 4 /*number of vertexes*/ * 4 /*floats per vertex*/ * sizeof(float));
        VertexBufferLayout layout;

        layout.Push<float>(2);
        layout.Push<float>(2);
        va.AddBuffer(vb, layout);

        IndexBuffer ib(indices, 6);

        glm::mat4 proj = glm::ortho(-1.0f, 1.0f, -1.0f, 1.0f, 1.0f, -1.0f);
        glm::mat4 identity = glm::mat4(1.0f);

        std::filesystem::path shader_path = PathManager::getShaderPath() / "Basic.shader";
        std::filesystem::path grid_shader_path = PathManager::getShaderPath() / "GridShader.shader";
        Shader shader(shader_path.string());
        Shader grid_shader(grid_shader_path.string());

        grid_shader.Bind();
        grid_shader.SetUniformMat4f("projection", proj);
        grid_shader.SetUniformMat4f("view", identity);
        grid_shader.SetUniformMat4f("model", identity);
        grid_shader.SetUniform1i("grid_type", 0);
        grid_shader.SetUniform1f("cell_size", 1.0f);
        grid_shader.SetUniform2f("grid_offset", 1.0f, 1.0f);
        grid_shader.SetUniform1f("opacity", 1.0f);

        shader.Bind();
        shader.SetUniform1i("u_Texture", 0);
        shader.SetUniformMat4f("projection", proj);
        shader.SetUniformMat4f("view", identity);
        shader.SetUniformMat4f("model", identity);
        shader.SetUniform1i("u_UseTexture", 1);
        shader.SetUniform1f("u_Alpha", 0.5f);

        va.Unbind();
        vb.Unbind();
        ib.Unbind();
        shader.Unbind();

        Renderer renderer;

        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        (void)io;
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // Enable Keyboard Controls
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;  // Enable Gamepad Controls
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;     // Enable Docking
        io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;   // Enable Multi-Viewport / Platform Windows
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
            marker_directory->applyPendingAssetChanges();
            map_directory->applyPendingAssetChanges();
            game_table_manager->processReceivedMessages();
            TickAutoSave();

            /* Render here */
            GLCall(glClearColor(0.0f, 0.0f, 0.0f, 1.0f));
            GLCall(glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT));

            // Start the Dear ImGui frame
            ImGui_ImplOpenGL3_NewFrame();
            ImGui_ImplGlfw_NewFrame();
            ImGui::NewFrame();

            DebugConsole::Render();
            DebugConsole::RunActiveDebugToggles();

            renderDockSpace();
            renderMainMenuBar();
            renderMapFBO(va, ib, shader, grid_shader, renderer);
            renderActiveGametable();
            toaster_->Render();

            //ImGui::ShowMetricsWindow();
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

        map_directory->stopMonitoring();
        marker_directory->stopMonitoring();
    }

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}

void ApplicationHandler::renderDockSpace()
{
    auto dockspace_id = ImGui::GetID("Root");
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    if (g_dockspace_initialized == false)
    {
        g_dockspace_initialized = true;
        ImGui::DockBuilderRemoveNode(dockspace_id);
        ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_DockSpace | ImGuiDockNodeFlags_PassthruCentralNode);
        ImGui::DockBuilderSetNodeSize(dockspace_id, viewport->Size);
        ImGui::DockBuilderSetNodePos(dockspace_id, viewport->Pos);
        ImGuiID dock_panel_id;
        ImGuiID dock_right_id = ImGui::DockBuilderSplitNode(dockspace_id, ImGuiDir_Right, 0.3f, nullptr, &dock_panel_id);

        ImGui::DockBuilderDockWindow("MapWindow", dock_panel_id);
        ImGui::DockBuilderDockWindow("ChatWindow", dock_right_id);
        ImGui::DockBuilderDockWindow("MarkerDiretory", dock_right_id);
        ImGui::DockBuilderFinish(dockspace_id);
    }

    ImGuiWindowFlags root_flags = ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoMouseInputs | ImGuiWindowFlags_NoFocusOnAppearing /*<<--TESTING BEHAVIOUR FLAGS--*/
                                  | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoBringToFrontOnFocus;
    ImGui::SetNextWindowPos({viewport->Pos.x, viewport->Pos.y + ImGui::GetFrameHeight()}, ImGuiCond_Always);
    ImGui::SetNextWindowSize({viewport->Size.x, viewport->Size.y - ImGui::GetFrameHeight()}, ImGuiCond_Always); //TESTING WITHOUT ImGuiCond_Once
    ImGui::Begin("RootWindow", nullptr, root_flags);
    ImGui::DockSpace(dockspace_id);
    ImGui::End();
}

void ApplicationHandler::renderMapFBO(VertexArray& va, IndexBuffer& ib, Shader& shader, Shader& grid_shader, Renderer& renderer)
{
    if (map_fbo->fboID != 0 && map_fbo->width > 0 && map_fbo->height > 0)
    {
        GLCall(glBindFramebuffer(GL_FRAMEBUFFER, map_fbo->fboID));
        GLCall(glViewport(0, 0, map_fbo->width, map_fbo->height)); // Crucial: Viewport matches FBO size
        GLCall(glClearColor(0.0f, 0.0f, 0.0f, 0.0f));              // Clear FBO background
        GLCall(glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT));

        // Inform the camera about the FBO's current dimensions for projection
        game_table_manager->setCameraFboDimensions(glm::vec2(map_fbo->width, map_fbo->height));
        game_table_manager->render(va, ib, shader, grid_shader, renderer); // Your board_manager->render() call

        GLCall(glBindFramebuffer(GL_FRAMEBUFFER, 0)); // Unbind FBO, return to default framebuffer
    }
}

void ApplicationHandler::renderActiveGametable()
{
    if (game_table_manager->isGameTableActive())
    {

        ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoScrollbar;

        ImGui::SetNextWindowSizeConstraints(ImVec2(800, 600), ImVec2(FLT_MAX, FLT_MAX));
        ImGui::Begin("MapWindow", nullptr, window_flags);
        ImVec2 content_size = ImGui::GetContentRegionAvail();

        if (content_size.x > 0 && content_size.y > 0)
        {
            ResizeMapFBO(static_cast<int>(content_size.x), static_cast<int>(content_size.y));
        }
        ImVec2 window_size = ImGui::GetWindowSize();
        ImVec2 window_pos = ImGui::GetWindowPos();

        if (game_table_manager->isBoardActive())
        {
            if (map_fbo->textureID != 0)
            {
                ImGui::Image((void*)(intptr_t)map_fbo->textureID, content_size, ImVec2(0, 1), ImVec2(1, 0));

                ImVec2 displayed_image_size = ImGui::GetItemRectSize();
                ;
                ImVec2 image_min_screen_pos = ImGui::GetItemRectMin();

                ImVec2 toolbar_cursor_pos_in_parent = ImVec2(image_min_screen_pos.x - window_pos.x,
                                                             image_min_screen_pos.y - window_pos.y);
                game_table_manager->board_manager->renderToolbar(toolbar_cursor_pos_in_parent);

                ImGuiIO& io = ImGui::GetIO();
                ImVec2 mouse_pos_global = ImGui::GetMousePos();

                bool is_mouse_within_image_bounds = (mouse_pos_global.x >= image_min_screen_pos.x &&
                                                     mouse_pos_global.x < (image_min_screen_pos.x + displayed_image_size.x) &&
                                                     mouse_pos_global.y >= image_min_screen_pos.y &&
                                                     mouse_pos_global.y < (image_min_screen_pos.y + displayed_image_size.y));
                game_table_manager->processMouseInput(is_mouse_within_image_bounds);

                if (is_mouse_within_image_bounds)
                {
                    current_map_relative_mouse_pos = ImVec2(mouse_pos_global.x - image_min_screen_pos.x, mouse_pos_global.y - image_min_screen_pos.y);
                    float fbo_x = (current_map_relative_mouse_pos.x / displayed_image_size.x) * map_fbo->width;
                    float fbo_y = (current_map_relative_mouse_pos.y / displayed_image_size.y) * map_fbo->height;
                    current_fbo_mouse_pos = glm::vec2(fbo_x, fbo_y);
                    game_table_manager->handleInputs(current_fbo_mouse_pos);

                    //DEBUG purposes
                    if (g_draw_debug_circle)
                    {
                        DrawDebugCircle(mouse_pos_global, false, IM_COL32(0, 255, 0, 150), 10.0f);                                                                                          // Yellow for raw click
                        DrawDebugCircle(image_min_screen_pos, false, IM_COL32(0, 255, 0, 255), 10.0f);                                                                                      // Yellow for image pos
                        DrawDebugCircle(ImVec2(image_min_screen_pos.x + current_fbo_mouse_pos.x, image_min_screen_pos.y + current_fbo_mouse_pos.y), false, IM_COL32(255, 0, 0, 255), 5.0f); // Red for raw click
                    }
                }

                ImGui::SetCursorScreenPos(image_min_screen_pos);
                ImGui::InvisibleButton("##MapDropArea", displayed_image_size);
                // Handle the drop payload
                if (ImGui::BeginDragDropTarget())
                {
                    if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("MARKER_IMAGE"))
                    {

                        const DirectoryWindow::ImageData* markerImage = (const DirectoryWindow::ImageData*)payload->Data;

                        glm::vec2 world_position = game_table_manager->board_manager->camera.screenToWorldPosition(current_fbo_mouse_pos);
                        game_table_manager->board_manager->createMarker(markerImage->filename, markerImage->textureID, world_position, markerImage->size);
                    }
                    ImGui::EndDragDropTarget();
                }
            }
        }

        ImGui::End();
    }
}

void ApplicationHandler::renderMainMenuBar()
{
    // one-shot flags to open popups
    bool open_host_gametable = false;
    bool connect_to_gametable = false;
    bool close_current_gametable = false;

    bool open_network_center = false;
    bool open_username_change = false;

    bool open_create_board = false;
    bool close_current_board = false;
    bool load_active_board = false;

    bool open_remove_assets = false;

    bool about = false;
    bool guide = false;

    ImGui::BeginMainMenuBar();

    // ---------------- Game Table ----------------
    if (ImGui::BeginMenu("Game Table"))
    {
        if (ImGui::MenuItem("Host..."))
        {
            open_host_gametable = true;
        }
        if (ImGui::MenuItem("Connect..."))
        {
            connect_to_gametable = true;
        }

        if (game_table_manager->isGameTableActive())
        {
            if (ImGui::MenuItem("Save"))
            {
                game_table_manager->saveGameTable();
            }
            if (ImGui::MenuItem("Close"))
            {
                close_current_gametable = true;
            }
        }
        ImGui::EndMenu();
    }
    bool showNetwork = (game_table_manager->network_manager && game_table_manager->network_manager->getPeerRole() != Role::NONE);
    // ---------------- Network ----------------
    if (showNetwork)
    {
        if (ImGui::BeginMenu("Network"))
        {
            if (ImGui::MenuItem("Network Center"))
            {
                open_network_center = true;
            }

            if (ImGui::MenuItem("Change Username"))
            {
                open_username_change = true;
            }
            ImGui::EndMenu();
        }
    }

    // ---------------- Board ----------------
    if (game_table_manager->isGameTableActive())
    {
        if (ImGui::BeginMenu("Board"))
        {
            if (ImGui::MenuItem("Create"))
            {
                open_create_board = true;
            }
            if (ImGui::MenuItem("Open"))
            {
                load_active_board = true;
            }
            if (game_table_manager->board_manager->isBoardActive())
            {
                if (ImGui::MenuItem("Save"))
                {
                    auto board_folder_path = PathManager::getGameTablesPath() / game_table_manager->game_table_name / "Boards";
                    game_table_manager->board_manager->saveActiveBoard(board_folder_path);
                }
                if (ImGui::MenuItem("Close"))
                {
                    close_current_board = true;
                }
            }

            ImGui::EndMenu();
        }
    }
    if (ImGui::BeginMenu("Notes"))
    {
        bool vis = DebugConsole::isVisible();
        if (ImGui::MenuItem("Note Editor(NOTYET)", nullptr, vis))
        {
            DebugConsole::setVisible(!vis);
        }
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Assets"))
    {
        if (ImGui::MenuItem("Add Marker (from file)"))
        {
            std::filesystem::path dst;
            std::string err;
            if (!AssetIO::importFromPicker(AssetIO::AssetKind::Marker, &dst, &err))
            {
                // toast/log error if needed
                std::cerr << "Import marker failed: " << err << "\n";
                toaster_->Push(ImGuiToaster::Level::Error, "Imported Marker ERROR: " + err);
            }
            else
            {
                toaster_->Push(ImGuiToaster::Level::Good, "Imported Marker Successfully!!" + err);
            }
        }
        if (ImGui::MenuItem("Add Map (from file)"))
        {
            std::filesystem::path dst;
            std::string err;
            if (!AssetIO::importFromPicker(AssetIO::AssetKind::Map, &dst, &err))
            {
                // toast/log error if needed
                std::cerr << "Import map failed: " << err << "\n";
                toaster_->Push(ImGuiToaster::Level::Error, "Imported Map ERROR: " + err);
            }
            else
            {
                toaster_->Push(ImGuiToaster::Level::Good, "Imported Map Successfully!!" + err);
            }
        }
        if (ImGui::MenuItem("Remove Assets..."))
        {
            open_remove_assets = true;
        }
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Help"))
    {
        if (ImGui::MenuItem("Guide"))
        {
            guide = true;
        }

        if (ImGui::MenuItem("About"))
        {
            about = true;
        }

        bool vis = DebugConsole::isVisible();
        if (ImGui::MenuItem("Console", nullptr, vis))
        {
            DebugConsole::setVisible(!vis);
        }

        ImGui::EndMenu();
    }

    ImGui::EndMainMenuBar();

    // ---------------- Popups (open + render) ----------------

    if (open_remove_assets)
        ImGui::OpenPopup("DeleteAssets");
    if (ImGui::IsPopupOpen("DeleteAssets"))
        AssetIO::openDeleteAssetPopUp(toaster_);

    if (open_host_gametable)
        ImGui::OpenPopup("Host GameTable");
    if (ImGui::IsPopupOpen("Host GameTable"))
        game_table_manager->hostGameTablePopUp();

    if (connect_to_gametable)
        ImGui::OpenPopup("ConnectToGameTable");
    if (ImGui::IsPopupOpen("ConnectToGameTable"))
        game_table_manager->connectToGameTablePopUp();

    if (close_current_gametable)
        ImGui::OpenPopup("CloseGameTable");
    if (ImGui::IsPopupOpen("CloseGameTable"))
        game_table_manager->closeGameTablePopUp();

    if (open_network_center)
        ImGui::OpenPopup("Network Center");
    if (ImGui::IsPopupOpen("Network Center"))
        game_table_manager->networkCenterPopUp();

    if (open_username_change)
        ImGui::OpenPopup("Change Username");
    if (ImGui::IsPopupOpen("Change Username"))
        game_table_manager->renderUsernameChangePopup();

    if (open_create_board)
        ImGui::OpenPopup("CreateBoard");
    if (ImGui::IsPopupOpen("CreateBoard"))
        game_table_manager->createBoardPopUp();

    if (load_active_board)
        ImGui::OpenPopup("LoadBoard");
    if (ImGui::IsPopupOpen("LoadBoard"))
        game_table_manager->loadBoardPopUp();

    if (close_current_board)
        ImGui::OpenPopup("CloseBoard");
    if (ImGui::IsPopupOpen("CloseBoard"))
        game_table_manager->closeBoardPopUp();

    if (guide)
        ImGui::OpenPopup("Guide");
    if (ImGui::IsPopupOpen("Guide"))
        game_table_manager->guidePopUp();

    if (about)
        ImGui::OpenPopup("About");
    if (ImGui::IsPopupOpen("About"))
        game_table_manager->aboutPopUp();
}
