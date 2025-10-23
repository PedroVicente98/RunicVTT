#include "GameTableManager.h"
#include "imgui_internal.h"
#include "Serializer.h"
#include "SignalingServer.h"
#include "UPnPManager.h"
#include "Logger.h"
#include "random"

GameTableManager::GameTableManager(flecs::world ecs, std::shared_ptr<DirectoryWindow> map_directory, std::shared_ptr<DirectoryWindow> marker_directory) :
    ecs(ecs), identity_manager(std::make_shared<IdentityManager>()), network_manager(std::make_shared<NetworkManager>(ecs, identity_manager)), map_directory(map_directory), board_manager(std::make_shared<BoardManager>(ecs, network_manager, identity_manager, map_directory, marker_directory))
{
    identity_manager->loadMyIdentityFromFile();
    identity_manager->loadAddressBookFromFile();
    if (identity_manager->myUniqueId().empty())
    {
        // any stable generator you like — here is a tiny one-liner fallback:
        const std::string newUid = std::to_string(std::random_device{}()) + "-" + std::to_string(std::random_device{}());
        identity_manager->setMyIdentity(newUid, "Player");
    }
    chat_manager = std::make_shared<ChatManager>(network_manager, identity_manager);

    std::filesystem::path map_directory_path = PathManager::getMapsPath();
    map_directory->directoryName = "MapDiretory";
    map_directory->directoryPath = map_directory_path.string();
    map_directory->startMonitoring();
    map_directory->generateTextureIDs();
}

void GameTableManager::setup()
{
    network_manager->setup(board_manager, weak_from_this());
}

GameTableManager::~GameTableManager()
{
}

void GameTableManager::saveGameTable()
{
    createGameTableFile(active_game_table);
    chat_manager->saveCurrent();
}

void GameTableManager::loadGameTable(std::filesystem::path game_table_file_path)
{
    std::ifstream inFile(game_table_file_path, std::ios::binary);
    if (inFile)
    {
        std::vector<unsigned char> buffer((std::istreambuf_iterator<char>(inFile)), std::istreambuf_iterator<char>());
        inFile.close();

        size_t offset = 0;
        active_game_table = Serializer::deserializeGameTableEntity(buffer, offset, ecs);
        auto tableId = active_game_table.get<Identifier>()->id;
        game_table_name = active_game_table.get<GameTable>()->gameTableName;
        chat_manager->setActiveGameTable(tableId, game_table_name);

        ecs.defer_begin();
        try
        {
            active_game_table.children([&](flecs::entity child)
                                       {
                if (child.has<Board>()) {
                    auto texture = child.get_mut<TextureComponent>();
                    auto board_image = map_directory->getImageByPath(texture->image_path);
                    texture->textureID = board_image.textureID;
                    texture->size = board_image.size;

                    child.children([&](flecs::entity grand_child) {
                        if (grand_child.has<MarkerComponent>()) {
                            auto grand_child_texture = grand_child.get_mut<TextureComponent>();
                            auto marker_image = board_manager->marker_directory->getImageByPath(grand_child_texture->image_path);
                            grand_child_texture->textureID = marker_image.textureID;
                            grand_child_texture->size = marker_image.size;
                        }
                    });
                    board_manager->setActiveBoard(child);
                } });
        }
        catch (const std::exception&)
        {
            std::cout << "ERROR LOADING IMAGES" << std::endl;
        }
        ecs.defer_end();
        toaster_->Push(ImGuiToaster::Level::Good, "GameTable '" + game_table_name + "' Saved Successfuly!!");
    }
    else
    {
        toaster_->Push(ImGuiToaster::Level::Error, "Failed Saving GameTable!!!");
        std::cerr << "Failed to load GameTable from " << game_table_file_path.string() << std::endl;
    }
}

bool GameTableManager::isBoardActive()
{
    return board_manager->isBoardActive();
}

bool GameTableManager::isGameTableActive()
{
    return active_game_table.is_valid();
}

bool GameTableManager::isConnected() const
{
    return network_manager->isConnected();
}

void GameTableManager::processReceivedMessages()
{
    constexpr int kMaxPerFrame = 32; // avoid long stalls

    static uint64_t last = 0;
    uint64_t t = nowMs();
    if (t - last >= 1000)
    {
        network_manager->housekeepPeers();
        last = t;
    }

    network_manager->drainInboundRaw(kMaxPerFrame);
    network_manager->drainEvents();
    int processed = 0;
    msg::ReadyMessage m;
    while (processed < kMaxPerFrame && network_manager->tryPopReadyMessage(m))
    {
        Logger::instance().log("localtunnel", Logger::Level::Info, msg::DCtypeString(m.kind) + "RECEIVED ON PROCESS!!!");
        switch (m.kind)
        {
            case msg::DCType::Snapshot_GameTable:
            {
                if (!m.tableId || !m.name)
                    break;
                active_game_table = ecs.entity("GameTable")
                                        .set(GameTable{*m.name})
                                        .set(Identifier{*m.tableId});
                game_table_name = *m.name;
                chat_manager->setActiveGameTable(*m.tableId, *m.name);
                Logger::instance().log("localtunnel", Logger::Level::Info, "GameTable Created!!");

                break;
            }

            case msg::DCType::CommitBoard:
            {
                if (!m.boardId || !m.boardMeta)
                    break;
                GLuint tex = 0;
                glm::vec2 texSize{0, 0};
                if (m.bytes && !m.bytes->empty())
                {
                    auto image = board_manager->LoadTextureFromMemory(m.bytes->data(),
                                                                      m.bytes->size());
                    tex = image.textureID;
                    texSize = image.size;
                    Logger::instance().log("localtunnel", Logger::Level::Info, "Board Texture Created: " + std::to_string(tex));
                }

                const auto& bm = *m.boardMeta;
                auto board = ecs.entity()
                                 .set(Identifier{bm.boardId})
                                 .set(Board{bm.boardName})
                                 .set(Panning{false})
                                 .set(Grid{bm.grid})
                                 .set(TextureComponent{tex, "", texSize})
                                 .set(Size{texSize.x, texSize.y});

                board_manager->setActiveBoard(board);
                Logger::instance().log("localtunnel", Logger::Level::Info, "Board Created!!");
                break;
            }

            case msg::DCType::CommitMarker:
            {
                if (!m.boardId || !m.markerMeta)
                    break;
                auto boardEnt = board_manager->findBoardById(*m.boardId);
                if (!boardEnt.is_valid())
                    break;

                GLuint tex = 0;
                glm::vec2 texSize{m.markerMeta->size.width, m.markerMeta->size.height};
                Logger::instance().log("localtunnel", Logger::Level::Info, "Marker Texture Byte Size: " + std::to_string(m.bytes->size()));
                if (m.bytes && !m.bytes->empty())
                {
                    auto image = board_manager->LoadTextureFromMemory(m.bytes->data(),
                                                                      m.bytes->size());
                    tex = image.textureID;
                    texSize = image.size;
                    Logger::instance().log("localtunnel", Logger::Level::Info, "Marker Texture Created: " + std::to_string(tex));
                }
                const auto& mm = *m.markerMeta;
                flecs::entity marker = ecs.entity()
                                           .set(Identifier{mm.markerId})
                                           .set(Position{mm.pos.x, mm.pos.y}) //World Position
                                           .set(Size{mm.size.width, mm.size.height})
                                           .set(TextureComponent{tex, "", texSize})
                                           .set(Visibility{mm.vis})
                                           .set(MarkerComponent{"", "", false, false})
                                           .set(Moving{mm.mov});
                marker.add(flecs::ChildOf, boardEnt);
                Logger::instance().log("localtunnel", Logger::Level::Info, "Marker Created");
                break;
            }

            case msg::DCType::FogCreate:
            {
                if (!m.boardId || !m.fogId || !m.pos || !m.size || !m.vis)
                    break;
                auto boardEnt = board_manager->findBoardById(*m.boardId);
                if (!boardEnt.is_valid())
                    break;

                auto fog = ecs.entity()
                               .set(Identifier{*m.fogId})
                               .set(Position{m.pos->x, m.pos->y})
                               .set(Size{m.size->width, m.size->height})
                               .set(Visibility{*m.vis});

                fog.add<FogOfWar>();
                fog.add(flecs::ChildOf, boardEnt);
                Logger::instance().log("localtunnel", Logger::Level::Info, "Fog Created");
                break;
            }

            case msg::DCType::FogUpdate:
            {
                if (!m.boardId || !m.fogId)
                    break;
                auto boardEnt = board_manager->findBoardById(*m.boardId);
                if (!boardEnt.is_valid())
                    break;

                flecs::entity fogEnt;
                boardEnt.children([&](flecs::entity child)
                                  {
                    if (child.has<FogOfWar>()) {
                        auto id = child.get<Identifier>()->id;
                        if (id == *m.fogId) fogEnt = child;
                        } });
                if (!fogEnt.is_valid())
                    break;

                if (m.pos)
                    fogEnt.set<Position>(*m.pos);
                if (m.size)
                    fogEnt.set<Size>(*m.size);
                if (m.vis)
                    fogEnt.set<Visibility>(*m.vis);
                if (m.mov)
                    fogEnt.set<Moving>(*m.mov); // if used
                break;
            }

            case msg::DCType::FogDelete:
            {
                if (!m.boardId || !m.fogId)
                    break;
                auto boardEnt = board_manager->findBoardById(*m.boardId);
                if (!boardEnt.is_valid())
                    break;

                flecs::entity fogEnt;
                boardEnt.children([&](flecs::entity child)
                                  {
                    if (child.has<FogOfWar>()) {
                        auto id = child.get<Identifier>()->id;
                        if (id == *m.fogId) fogEnt = child;
                    } });
                if (fogEnt.is_valid())
                    fogEnt.destruct();
                break;
            }

            case msg::DCType::MarkerMove:
            {
                // Epoch/seq gate (no side effects)
                if (!network_manager->shouldApplyMarkerMove(m))
                    break;

                if (!m.boardId || !m.markerId || !m.pos)
                    break;

                auto boardEnt = board_manager->findBoardById(*m.boardId);
                if (!boardEnt.is_valid())
                    break;

                auto markerEnt = findMarkerInBoard(boardEnt, *m.markerId);
                if (!markerEnt.is_valid())
                    break;

                // Apply streaming position; keep Moving true during drag
                markerEnt.set<Position>(*m.pos);
                //markerEnt.set<Moving>(Moving{true});
                break;
            }

            case msg::DCType::MarkerMoveState:
            {
                if (!m.boardId || !m.markerId)
                    break;

                auto boardEnt = board_manager->findBoardById(*m.boardId);
                if (!boardEnt.is_valid())
                    break;

                auto markerEnt = findMarkerInBoard(boardEnt, *m.markerId);
                if (!markerEnt.is_valid())
                    break;

                // Start of drag
                if (m.mov && m.mov->isDragging)
                {
                    if (!network_manager->shouldApplyMarkerMoveStateStart(m))
                        break;

                    // optional visual sync (safe; local drags are already set locally)
                    markerEnt.set<Moving>(Moving{true});
                }
                else // End of drag (final)
                {
                    if (!network_manager->shouldApplyMarkerMoveStateFinal(m))
                        break;

                    if (m.pos)
                        markerEnt.set<Position>(*m.pos);
                    markerEnt.set<Moving>(Moving{false}); // ensure drag ends
                }
                break;
            }

            case msg::DCType::MarkerUpdate:
            {
                if (!m.boardId || !m.markerId)
                    break;

                auto boardEnt = board_manager->findBoardById(*m.boardId);
                if (!boardEnt.is_valid())
                    break;

                auto markerEnt = findMarkerInBoard(boardEnt, *m.markerId);
                if (!markerEnt.is_valid())
                    break;

                // Apply only non-movement attributes
                if (m.size)
                    markerEnt.set<Size>(*m.size);
                if (m.vis)
                    markerEnt.set<Visibility>(*m.vis);
                if (m.markerComp)
                {
                    std::string oldOwnerUid = markerEnt.get<MarkerComponent>()->ownerUniqueId;
                    if (oldOwnerUid != m.markerComp->ownerUniqueId)
                        network_manager->drag_.erase(*m.markerId);
                    markerEnt.set<MarkerComponent>(*m.markerComp);
                }

                break;
            }

            case msg::DCType::MarkerDelete:
            {
                if (!m.boardId || !m.markerId)
                    break;
                auto boardEnt = board_manager->findBoardById(*m.boardId);
                if (!boardEnt.is_valid())
                    break;

                flecs::entity markerEnt;
                boardEnt.children([&](flecs::entity child)
                                  {
                    if (child.has<MarkerComponent>()) {
                        auto id = child.get<Identifier>()->id;
                        if (id == *m.markerId) markerEnt = child;
                    } });
                if (markerEnt.is_valid())
                    markerEnt.destruct();
                break;
            }

            // GameTableManager.cpp — inside processReceivedMessages() switch:
            case msg::DCType::UserNameUpdate:
            {
                if (!m.tableId || !m.userUniqueId || !m.name || !m.text)
                    break;
                if (*m.tableId != chat_manager->currentTableId_)
                    break;

                const std::string uniqueId = *m.userUniqueId; // now uniqueId
                const std::string fromPeerId = m.fromPeerId;
                const std::string newU = *m.name;

                // 1) record in address book
                network_manager->upsertPeerIdentityWithUnique(/*peerId=*/m.fromPeerId, /*uniqueId=*/uniqueId, /*username=*/newU);
                identity_manager->setUsernameForUnique(uniqueId, newU);
                board_manager->onUsernameChanged(uniqueId, newU);
                chat_manager->replaceUsernameForUnique(uniqueId, newU);

                break;
            }

            case msg::DCType::ChatGroupCreate:
            {
                chat_manager->applyReady(m);
                break;
            }
            case msg::DCType::ChatGroupUpdate:
            {
                chat_manager->applyReady(m);
                break;
            }
            case msg::DCType::ChatGroupDelete:
            {
                chat_manager->applyReady(m);
                break;
            }
            case msg::DCType::ChatMessage:
            {
                chat_manager->applyReady(m);
                break;
            }

            case msg::DCType::GridUpdate:
            {
                if (!m.boardId || !m.grid)
                    break;

                auto boardEnt = board_manager->findBoardById(*m.boardId);
                if (!boardEnt.is_valid())
                    break;

                boardEnt.set<Grid>(*m.grid);
                break;
            }

            case msg::DCType::NoteCreate:
            {
                break;
            }

            case msg::DCType::NoteUpdate:
            {
                break;
            }

            case msg::DCType::NoteDelete:
            {
                break;
            }

            default:
                break;
        }

        ++processed;
    }
}

void GameTableManager::setCameraFboDimensions(glm::vec2 fbo_dimensions)
{
    board_manager->camera.setFboDimensions(fbo_dimensions);
};

void GameTableManager::handleInputs(glm::vec2 current_mouse_fbo_pos)
{
    current_mouse_world_pos = board_manager->camera.screenToWorldPosition(current_mouse_fbo_pos);
    this->current_mouse_fbo_pos = current_mouse_fbo_pos;

    // Call individual handlers
    handleMouseButtonInputs();
    handleCursorInputs(); // This will primarily deal with dragging/panning
    handleScrollInputs();
}

void GameTableManager::handleMouseButtonInputs()
{
    // Check if board is active first
    if (!isBoardActive())
        return;

    // Left Mouse Button Press
    if (mouse_left_clicked)
    {
        if (board_manager->getCurrentTool() == Tool::MOVE)
        {
            if (board_manager->isMouseOverMarker(current_mouse_world_pos) /*and !board_manager->isDraggingMarker()*/)
            {
                board_manager->startMouseDrag(current_mouse_world_pos, false); // Drag Marker
            }
            else /*if(!board_manager->isPanning())*/
            {
                board_manager->startMouseDrag(current_mouse_world_pos, true); // Pan Board
            }
        }
        if (board_manager->getCurrentTool() == Tool::FOG and !board_manager->isCreatingFog())
        {
            board_manager->startMouseDrag(current_mouse_world_pos, false); // Start fog drawing/erasing
        }
        if (board_manager->getCurrentTool() == Tool::SELECT)
        {
            auto entity = board_manager->getEntityAtMousePosition(current_mouse_world_pos);
            if (entity.is_valid())
            {
                board_manager->setShowEditWindow(true, entity);
            }
        }
    }

    // Left Mouse Button Release
    if (mouse_left_released || ImGui::IsMouseReleased(ImGuiMouseButton_Left))
    {
        if (board_manager->isPanning() || board_manager->isDraggingMarker())
        {
            board_manager->endMouseDrag();
        }
        if (board_manager->isCreatingFog())
        {
            board_manager->handleFogCreation(current_mouse_world_pos); // Use world_pos
            board_manager->endMouseDrag();
        }
    }

    board_manager->killIfMouseUp(ImGui::IsMouseDown(ImGuiMouseButton_Left));
}

void GameTableManager::handleCursorInputs()
{
    if (!isBoardActive())
        return;

    if (board_manager->isDraggingMarker())
    {
        board_manager->handleMarkerDragging(current_mouse_world_pos);
    }

    if (board_manager->isPanning())
    {
        board_manager->panBoard(current_mouse_fbo_pos);
    }
}

void GameTableManager::handleScrollInputs()
{
    if (!isBoardActive())
        return;
    if (mouse_wheel_delta != 0.0f)
    {
        float zoom_factor;
        if (mouse_wheel_delta > 0)
        {
            zoom_factor = 1.1f; // Zoom in by 10%
        }
        else
        {
            zoom_factor = 0.9f; // Zoom out by 10%
        }
        board_manager->camera.zoom(zoom_factor, current_mouse_world_pos);
    }
}

void GameTableManager::createGameTableFile(flecs::entity game_table)
{
    namespace fs = std::filesystem;
    auto game_tables_directory = PathManager::getGameTablesPath();
    auto active_game_table_folder = game_tables_directory / game_table_name;
    if (!fs::exists(active_game_table_folder) && !fs::is_directory(active_game_table_folder))
    {
        std::filesystem::create_directory(active_game_table_folder);
    }

    std::vector<unsigned char> buffer;
    Serializer::serializeGameTableEntity(buffer, game_table, ecs);

    auto game_table_file = active_game_table_folder / (game_table_name + ".runic");
    std::ofstream file(game_table_file.string(), std::ios::binary);
    if (file.is_open())
    {
        file.write(reinterpret_cast<const char*>(buffer.data()), buffer.size());
        file.close();
    }
    else
    {
        std::cerr << "Failed to open the file." << std::endl;
    }
}

//
std::vector<std::string> GameTableManager::listBoardFiles()
{
    namespace fs = std::filesystem;
    auto game_tables_directory = PathManager::getGameTablesPath();
    auto active_game_table_folder = game_tables_directory / game_table_name;
    if (!fs::exists(active_game_table_folder) && !fs::is_directory(active_game_table_folder))
    {
        std::filesystem::create_directory(active_game_table_folder);
    }
    auto game_table_boards_folder = active_game_table_folder / "Boards";

    if (!fs::exists(game_table_boards_folder) && !fs::is_directory(game_table_boards_folder))
    {
        std::filesystem::create_directory(game_table_boards_folder);
    }
    std::vector<std::string> boards;
    for (const auto& entry : std::filesystem::directory_iterator(game_table_boards_folder))
    {
        if (entry.is_regular_file())
        {
            boards.emplace_back(entry.path().filename().string());
        }
    }

    return boards;
}

std::vector<std::string> GameTableManager::listGameTableFiles()
{
    namespace fs = std::filesystem;
    auto game_tables_directory = PathManager::getGameTablesPath();

    // Verifica se o diretÃ³rio "GameTables" existe
    if (!fs::exists(game_tables_directory) || !fs::is_directory(game_tables_directory))
    {
        std::cerr << "GameTables directory does not exist!" << std::endl;
        return {};
    }
    std::vector<std::string> game_tables;
    // Itera sobre todos os diretÃ³rios dentro de "GameTables"
    for (const auto& folder : fs::directory_iterator(game_tables_directory))
    {
        if (folder.is_directory())
        {
            std::string folder_name = folder.path().filename().string();
            auto runic_file_path = folder.path() / (folder_name + ".runic");

            // Verifica se o arquivo "folder_name.runic" existe dentro da pasta
            if (fs::exists(runic_file_path) && fs::is_regular_file(runic_file_path))
            {
                game_tables.emplace_back(runic_file_path.filename().string()); // Adiciona o nome do arquivo na lista
            }
        }
    }

    return game_tables;
}

// ----------------------------- GUI --------------------------------------------------------------------------------

void GameTableManager::createBoardPopUp()
{
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    if (ImGui::BeginPopupModal("CreateBoard"))
    {
        ImGui::SetItemDefaultFocus();
        ImGuiID dockspace_id = ImGui::GetID("CreateBoardDockspace");

        if (ImGui::DockBuilderGetNode(dockspace_id) == 0)
        {
            ImGui::DockBuilderRemoveNode(dockspace_id);
            ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_DockSpace | ImGuiDockNodeFlags_PassthruCentralNode | ImGuiDockNodeFlags_NoUndocking);

            ImGui::DockBuilderDockWindow("MapDiretory", dockspace_id);
            ImGui::DockBuilderFinish(dockspace_id);
        }

        ImGui::Columns(2, nullptr, false);

        ImGui::Text("Create a new board");

        ImGui::InputText("Board Name", buffer, sizeof(buffer));
        std::string board_name(buffer);

        ImGui::NewLine();

        DirectoryWindow::ImageData selectedImage = map_directory->getSelectedImage();

        if (!selectedImage.filename.empty())
        {
            ImGui::Text("Selected Map: %s", selectedImage.filename.c_str());
            ImGui::Image((void*)(intptr_t)selectedImage.textureID, ImVec2(256, 256));
        }
        else
        {
            ImGui::Text("No map selected. Please select a map.");
        }

        ImGui::NewLine();

        bool saved = false;
        if (ImGui::Button("Save") && !selectedImage.filename.empty() && buffer[0] != '\0')
        {
            auto board = board_manager->createBoard(board_name, selectedImage.filename, selectedImage.textureID, selectedImage.size);
            board.add(flecs::ChildOf, active_game_table);
            map_directory->clearSelectedImage();
            memset(buffer, 0, sizeof(buffer));

            network_manager->broadcastBoard(board_manager->getActiveBoard());
            saved = true;
            ImGui::CloseCurrentPopup();
        }

        ImGui::SameLine();

        if (ImGui::Button("Close"))
        {
            map_directory->clearSelectedImage();
            ImGui::CloseCurrentPopup();
        }

        ImGui::NextColumn();
        ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_PassthruCentralNode);
        map_directory->renderDirectory();

        ImGui::Columns(1);
        // Optionally show transient "Saved!"
        UI_TransientLine("board-saved", saved, ImVec4(0.4f, 1.f, 0.4f, 1.f), "Saved!", 1.5f);

        ImGui::EndPopup();
    }
}

void GameTableManager::closeBoardPopUp()
{
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    if (ImGui::BeginPopupModal("CloseBoard", 0, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::Text("Close Current GameTable?? Any unsaved changes will be lost!!");
        if (ImGui::Button("Close"))
        {
            board_manager->closeBoard();
            ImGui::CloseCurrentPopup();
        }

        ImGui::SameLine();

        if (ImGui::Button("Cancel"))
        {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

void GameTableManager::saveBoardPopUp()
{
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    if (ImGui::BeginPopupModal("SaveBoard", 0, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::Text("Save current board?");
        ImGui::NewLine();

        bool saved = false;
        if (ImGui::Button("Save"))
        {
            // your save logic here (you already said logic functions exist)
            saved = true;
            ImGui::CloseCurrentPopup();
        }

        ImGui::SameLine();

        if (ImGui::Button("Close"))
        {
            ImGui::CloseCurrentPopup();
        }

        UI_TransientLine("save-board-ok", saved, ImVec4(0.4f, 1.f, 0.4f, 1.f), "Saved!", 1.5f);

        ImGui::EndPopup();
    }
}

void GameTableManager::loadBoardPopUp()
{
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    if (ImGui::BeginPopupModal("LoadBoard", 0, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::Text("Load Board:");

        ImGui::NewLine();
        ImGui::Separator();

        auto boards = listBoardFiles();
        bool loaded = false;
        for (auto& board : boards)
        {
            if (ImGui::Button(board.c_str()))
            {
                std::filesystem::path board_file_path = PathManager::getRootDirectory() / "GameTables" / game_table_name / "Boards" / board;
                board_manager->loadActiveBoard(board_file_path.string());
                network_manager->broadcastBoard(board_manager->getActiveBoard());
                loaded = true;
                ImGui::CloseCurrentPopup();
            }
        }

        UI_TransientLine("board-loaded", loaded, ImVec4(0.4f, 1.f, 0.4f, 1.f), "Board Loaded!", 1.5f);

        ImGui::NewLine();
        ImGui::Separator();

        if (ImGui::Button("Close"))
        {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

void GameTableManager::closeGameTablePopUp()
{
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    if (ImGui::BeginPopupModal("CloseGameTable", 0, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::Text("Close Current GameTable?? Any unsaved changes will be lost!!");
        if (ImGui::Button("Close"))
        {
            board_manager->closeBoard();
            active_game_table = flecs::entity();
            network_manager->closeServer();
            chat_manager->saveCurrent();
            ImGui::CloseCurrentPopup();
        }

        ImGui::SameLine();

        if (ImGui::Button("Cancel"))
        {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

void GameTableManager::connectToGameTablePopUp()
{
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

    if (ImGui::BeginPopupModal("ConnectToGameTable", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::SetItemDefaultFocus();

        // Username
        ImGui::TextUnformatted("Enter your username and the connection string you received from the host.");
        ImGui::InputText("Username", username_buffer, sizeof(username_buffer));

        ImGui::Separator();

        // Connection string
        // Expected formats:
        //   runic:https://sub.loca.lt?PASSWORD
        //   runic:wss://host[:port/path]?PASSWORD
        //   runic:<host>:<port>?PASSWORD
        ImGui::InputText("Connection String", buffer, sizeof(buffer), ImGuiInputTextFlags_AutoSelectAll);

        // Small helper row
        ImGui::BeginDisabled(true);
        ImGui::InputText("Example", (char*)"runic:https://xyz.loca.lt?mypwd", 64);
        ImGui::EndDisabled();

        ImGui::Separator();

        // UX actions
        bool connectFailed = false;

        if (ImGui::Button("Connect") && buffer[0] != '\0')
        {
            // set username (id will be assigned after auth)
            auto my_unique = identity_manager->myUniqueId();
            identity_manager->setMyIdentity(my_unique, username_buffer);

            // this already accepts LocalTunnel URLs or host:port (parseConnectionString handles both)
            if (network_manager->connectToPeer(buffer))
            {
                // cleanup & close
                memset(buffer, 0, sizeof(buffer));
                memset(username_buffer, 0, sizeof(username_buffer));
                ImGui::CloseCurrentPopup();
            }
            else
            {
                connectFailed = true;
            }
        }

        ImGui::SameLine();
        if (ImGui::Button("Close"))
        {
            ImGui::CloseCurrentPopup();
            memset(buffer, 0, sizeof(buffer));
        }

        // transient error (uses your helper)
        UI_TransientLine("conn-fail",
                         connectFailed,
                         ImVec4(1.f, 0.f, 0.f, 1.f),
                         "Failed to Connect! Check your connection string and reachability.",
                         2.5f);

        // Helpful hint (host-side details)
        ImGui::Separator();
        ImGui::TextDisabled("Tip: The host chooses the network mode while hosting.\n"
                            "LocalTunnel URLs may take a few seconds to become available.");

        ImGui::EndPopup();
    }
}

// ================== 1) Router popup (keeps your STATUS header) ==================
void GameTableManager::networkCenterPopUp()
{
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

    if (ImGui::BeginPopupModal("Network Center", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {

        // ---------- STATUS (keep this block) ----------
        Role role = network_manager->getPeerRole();
        const char* roleStr =
            role == Role::GAMEMASTER ? "Hosting (GM)" : role == Role::PLAYER ? "Connected (Player)"
                                                                             : "Idle";
        ImVec4 roleClr =
            role == Role::GAMEMASTER ? ImVec4(0.2f, 0.9f, 0.2f, 1) : role == Role::PLAYER ? ImVec4(0.2f, 0.6f, 1.0f, 1)
                                                                                          : ImVec4(1.0f, 0.6f, 0.2f, 1);
        ImGui::Text("Status: ");
        ImGui::SameLine();
        ImGui::TextColored(roleClr, "%s", roleStr);

        ImGui::Separator();

        // ---------- BODY ----------
        if (role == Role::GAMEMASTER)
        {
            renderNetworkCenterGM(); // full GM center
        }
        else if (role == Role::PLAYER)
        {
            renderNetworkCenterPlayer(); // player center (read-only)
        }
        else
        {
            ImGui::TextDisabled("No network connection established.");
        }

        ImGui::Separator();
        if (ImGui::Button("Close"))
            ImGui::CloseCurrentPopup();

        ImGui::EndPopup();
    }
}

void GameTableManager::renderNetworkCenterGM()
{
    // ---------- INFO ----------
    const auto local_ip = network_manager->getLocalIPAddress();
    const auto external_ip = network_manager->getExternalIPAddress();
    const auto port = network_manager->getPort();
    const auto cs_local = network_manager->getNetworkInfo(ConnectionType::LOCAL);
    const auto cs_external = network_manager->getNetworkInfo(ConnectionType::EXTERNAL);
    const auto cs_lt = network_manager->getNetworkInfo(ConnectionType::LOCALTUNNEL);

    ImGui::TextUnformatted("Local IP:");
    ImGui::SameLine();
    ImGui::TextUnformatted(local_ip.c_str());
    ImGui::TextUnformatted("External IP:");
    ImGui::SameLine();
    ImGui::TextUnformatted(external_ip.c_str());
    ImGui::TextUnformatted("Port:");
    ImGui::SameLine();
    ImGui::Text("%u", port);

    ImGui::Separator();

    auto copyRow = [this](const char* label, const std::string& value,
                          const char* btnId, const char* toastId)
    {
        ImGui::TextUnformatted(label);
        ImGui::SameLine();
        ImGui::TextUnformatted(value.c_str());
        ImGui::SameLine();
        UI_CopyButtonWithToast(btnId, value, toastId, 1.5f);
    };

    copyRow("LocalTunnel URL:", cs_lt, "Copy##lt", "toast-lt");
    copyRow("Local Connection String:", cs_local, "Copy##loc", "toast-loc");
    copyRow("External Connection String:", cs_external, "Copy##ext", "toast-ext");

    // ---------- PLAYERS (P2P) ----------
    ImGui::Separator();
    ImGui::Text("Players (P2P)");
    if (ImGui::BeginTable("PeersTable", 5, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp))
    {
        ImGui::TableSetupColumn("Username", ImGuiTableColumnFlags_WidthStretch, 1.5f);
        ImGui::TableSetupColumn("Peer ID", ImGuiTableColumnFlags_WidthStretch, 2.0f);
        ImGui::TableSetupColumn("PC State", ImGuiTableColumnFlags_WidthFixed, 100.f);
        ImGui::TableSetupColumn("DataChannel", ImGuiTableColumnFlags_WidthFixed, 100.f);
        ImGui::TableSetupColumn("Actions", ImGuiTableColumnFlags_WidthFixed, 120.f);
        ImGui::TableHeadersRow();

        for (auto& [peerId, link] : network_manager->getPeers())
        {
            if (!link)
                continue;

            ImGui::TableNextRow();

            // Username
            ImGui::TableSetColumnIndex(0);
            ImGui::TextUnformatted(network_manager->displayNameForPeer(peerId).c_str());

            // Peer ID
            ImGui::TableSetColumnIndex(1);
            ImGui::TextUnformatted(peerId.c_str());

            // PC State (with color)
            ImGui::TableSetColumnIndex(2);
            const char* pcStr = link->pcStateString();
            ImVec4 pcCol = ImVec4(0.8f, 0.8f, 0.8f, 1);
            if (strcmp(pcStr, "Connected") == 0)
                pcCol = ImVec4(0.3f, 1.0f, 0.3f, 1);
            else if (strcmp(pcStr, "Connecting") == 0)
                pcCol = ImVec4(1.0f, 0.8f, 0.2f, 1);
            else if (strcmp(pcStr, "Disconnected") == 0 || strcmp(pcStr, "Failed") == 0)
                pcCol = ImVec4(1.0f, 0.3f, 0.3f, 1);
            ImGui::TextColored(pcCol, "%s", pcStr);

            // DC state
            ImGui::TableSetColumnIndex(3);
            const bool dcOpen = link->isDataChannelOpen();
            ImGui::TextUnformatted(dcOpen ? "Open" : "Closed");

            ImGui::TableSetColumnIndex(4);

            ImGui::PushID(peerId.c_str());
            if (ImGui::SmallButton("Disconnect"))
            {
                ImGui::OpenPopup("ConfirmKickPeer");
            }
            if (UI_ConfirmModal("ConfirmKickPeer", "Disconnect this peer?",
                                "This will disconnect the selected peer and notify others to drop it."))
            {
                // 1) Broadcast PeerDisconnect (so other peers drop links to this id)
                network_manager->broadcastPeerDisconnect(peerId);
                // 2) Optionally kick WS client too (server-side)
                if (auto srv = network_manager->getSignalingServer())
                {
                    srv->disconnectClient(peerId); // add this helper to server if not present
                }
                // 3) Locally close our peer link
                network_manager->removePeer(peerId);
            }
            ImGui::PopID();
        }
        ImGui::EndTable();
    }

    // ---------- CLIENTS (WS) ----------
    ImGui::Separator();
    ImGui::Text("Clients (WebSocket)");
    if (ImGui::BeginTable("ClientsTable", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp))
    {
        ImGui::TableSetupColumn("ClientId");
        ImGui::TableSetupColumn("Username");
        ImGui::TableSetupColumn("Actions");
        ImGui::TableHeadersRow();

        if (auto srv = network_manager->getSignalingServer())
        {
            for (auto& [cid, ws] : srv->authClients())
            {
                ImGui::TableNextRow();

                ImGui::TableSetColumnIndex(0);
                ImGui::TextUnformatted(cid.c_str());

                ImGui::TableSetColumnIndex(1);
                ImGui::TextUnformatted(network_manager->displayNameForPeer(cid).c_str());

                ImGui::TableSetColumnIndex(2);
                if (ImGui::SmallButton((std::string("Disconnect##") + cid).c_str()))
                {
                    srv->disconnectClient(cid);
                }
            }
        }
        else
        {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::TextDisabled("No signaling server");
        }
        ImGui::EndTable();
    }

    // ---------- Controls ----------
    ImGui::Separator();
    if (ImGui::Button("Disconnect All"))
    {
        ImGui::OpenPopup("ConfirmGMDisconnectAll");
    }
    ImGui::SameLine();
    if (UI_ConfirmModal("ConfirmGMDisconnectAll", "Disconnect ALL clients?",
                        "This will broadcast a shutdown and disconnect all clients, "
                        "close all peer links, and stop the signaling server."))
    {
        network_manager->disconnectAllPeers(); // your GM teardown
        ImGui::CloseCurrentPopup();
    }

    if (ImGui::Button("Close Network"))
    {
        ImGui::OpenPopup("ConfirmCloseNetwork");
    }

    if (UI_ConfirmModal("ConfirmCloseNetwork", "Close Network?",
                        "This close will close the server"
                        "and stop the signaling server."))
    {
        network_manager->closeServer();
        ImGui::CloseCurrentPopup();
    }
}

void GameTableManager::renderNetworkCenterPlayer()
{
    // Username
    ImGui::Text("Username: %s", network_manager->getMyUsername().c_str());
    ImGui::NewLine();

    // Peers (read-only)
    if (ImGui::BeginTable("PeersPlayer", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp))
    {
        ImGui::TableSetupColumn("Username", ImGuiTableColumnFlags_WidthStretch, 1.5f);
        ImGui::TableSetupColumn("Peer ID", ImGuiTableColumnFlags_WidthStretch, 2.0f);
        ImGui::TableSetupColumn("PC State", ImGuiTableColumnFlags_WidthFixed, 100.f);
        ImGui::TableSetupColumn("DataChannel", ImGuiTableColumnFlags_WidthFixed, 100.f);
        ImGui::TableHeadersRow();

        for (auto& [peerId, link] : network_manager->getPeers())
        {
            if (!link)
                continue;

            ImGui::TableNextRow();

            // Username
            ImGui::TableSetColumnIndex(0);
            ImGui::TextUnformatted(network_manager->displayNameForPeer(peerId).c_str());

            // Peer ID
            ImGui::TableSetColumnIndex(1);
            ImGui::TextUnformatted(peerId.c_str());

            // PC state
            ImGui::TableSetColumnIndex(2);
            const char* pcStr = link->pcStateString();
            ImVec4 pcCol = ImVec4(0.8f, 0.8f, 0.8f, 1);
            if (strcmp(pcStr, "Connected") == 0)
                pcCol = ImVec4(0.3f, 1.0f, 0.3f, 1);
            else if (strcmp(pcStr, "Connecting") == 0)
                pcCol = ImVec4(1.0f, 0.8f, 0.2f, 1);
            else if (strcmp(pcStr, "Disconnected") == 0 || strcmp(pcStr, "Failed") == 0)
                pcCol = ImVec4(1.0f, 0.3f, 0.3f, 1);
            ImGui::TextColored(pcCol, "%s", pcStr);

            // DC state
            ImGui::TableSetColumnIndex(3);
            ImGui::TextUnformatted(link->isDataChannelOpen() ? "Open" : "Closed");
        }

        ImGui::EndTable();
    }
    // At end of renderNetworkCenterPlayer()
    if (ImGui::Button("Disconnect"))
    {
        ImGui::OpenPopup("ConfirmPlayerDisconnect");
    }
    if (UI_ConfirmModal("ConfirmPlayerDisconnect", "Disconnect?",
                        "This will close your WebSocket and all peer connections."))
    {
        network_manager->disconectFromPeers(); // your player teardown
        ImGui::CloseCurrentPopup();            // close Network Center
    }
}

// ======================= Host GameTable (Create / Load + Network) =======================
void GameTableManager::hostGameTablePopUp()
{
    static ConnectionType hostMode = ConnectionType::LOCALTUNNEL;
    static bool tryUpnp = false; // only used for EXTERNAL

    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

    if (ImGui::BeginPopupModal("Host GameTable", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {

        if (ImGui::BeginTabBar("HostTabs"))
        {

            // ----------------------- CREATE TAB -----------------------
            if (ImGui::BeginTabItem("Create"))
            {

                ImGui::InputText("GameTable Name", buffer, sizeof(buffer));
                ImGui::InputText("Username", username_buffer, sizeof(username_buffer));
                ImGui::InputText("Password", pass_buffer, sizeof(pass_buffer), ImGuiInputTextFlags_Password);
                ImGui::InputText("Port", port_buffer, sizeof(port_buffer),
                                 ImGuiInputTextFlags_CharsDecimal | ImGuiInputTextFlags_CharsNoBlank);

                // Mode selection
                ImGui::Separator();
                ImGui::TextUnformatted("Connection Mode:");
                int m = (hostMode == ConnectionType::LOCALTUNNEL ? 0 : hostMode == ConnectionType::LOCAL ? 1
                                                                                                         : 2);
                if (ImGui::RadioButton("LocalTunnel", m == 0))
                    m = 0;
                ImGui::SameLine();
                if (ImGui::RadioButton("Local (LAN)", m == 1))
                    m = 1;
                ImGui::SameLine();
                if (ImGui::RadioButton("External (Internet)", m == 2))
                    m = 2;
                hostMode = (m == 0 ? ConnectionType::LOCALTUNNEL : m == 1 ? ConnectionType::LOCAL
                                                                          : ConnectionType::EXTERNAL);

                // Explanations
                if (hostMode == ConnectionType::LOCALTUNNEL)
                {
                    ImGui::TextDisabled("LocalTunnel: exposes your local server via a public URL.\n"
                                        "The URL becomes available a few seconds after the server starts.\n"
                                        "You can copy the connection string from Network Center.");
                    tryUpnp = false;
                }
                else if (hostMode == ConnectionType::LOCAL)
                {
                    ImGui::TextDisabled("Local (LAN): works only on the same local network.\n"
                                        "Share your LAN IP + port with players.");
                    tryUpnp = false;
                }
                else
                { // EXTERNAL
                    ImGui::TextDisabled("External: reachable from the Internet.\n"
                                        "Requires a public IP and port forwarding on your router (UPnP or manual).\n"
                                        "It might not work depending on your network.");
                    ImGui::Checkbox("Try UPnP (auto port forward)", &tryUpnp);
                }

                ImGui::Separator();

                const bool valid = buffer[0] != '\0' && port_buffer[0] != '\0';
                if (!valid)
                {
                    ImGui::TextColored(ImVec4(1, 0.6f, 0.2f, 1), "Name and Port are required.");
                }

                if (ImGui::Button("Create & Host") && valid)
                {
                    // Close whatever is running
                    board_manager->closeBoard();
                    active_game_table = flecs::entity();
                    network_manager->closeServer();

                    // Create GT entity + file
                    game_table_name = buffer;
                    auto identifier = Identifier{board_manager->generateUniqueId()};
                    auto game_table = ecs.entity("GameTable").set(identifier).set(GameTable{game_table_name});
                    active_game_table = game_table;
                    chat_manager->setActiveGameTable(identifier.id, game_table_name);
                    createGameTableFile(game_table);

                    // Identity + network
                    auto my_unique = identity_manager->myUniqueId();
                    identity_manager->setMyIdentity(my_unique, username_buffer);

                    network_manager->setNetworkPassword(pass_buffer);
                    const unsigned p = static_cast<unsigned>(atoi(port_buffer));
                    network_manager->startServer(hostMode, static_cast<unsigned short>(p), tryUpnp);

                    // cleanup + close
                    memset(buffer, 0, sizeof(buffer));
                    memset(username_buffer, 0, sizeof(username_buffer));
                    memset(pass_buffer, 0, sizeof(pass_buffer));
                    //memset(port_buffer, 0, sizeof(port_buffer));
                    tryUpnp = false;

                    ImGui::CloseCurrentPopup();
                }

                ImGui::SameLine();
                if (ImGui::Button("Cancel"))
                {
                    ImGui::CloseCurrentPopup();
                }

                ImGui::EndTabItem();
            }

            // ------------------------ LOAD TAB ------------------------
            if (ImGui::BeginTabItem("Load"))
            {
                ImGui::BeginChild("GTList", ImVec2(260, 360), true);
                auto game_tables = listGameTableFiles();
                for (auto& file : game_tables)
                {
                    if (ImGui::Selectable(file.c_str()))
                    {
                        strncpy(buffer, file.c_str(), sizeof(buffer) - 1);
                        buffer[sizeof(buffer) - 1] = '\0';
                    }
                }
                ImGui::EndChild();

                ImGui::SameLine();

                ImGui::BeginChild("GTDetails", ImVec2(420, 360), true);
                ImGui::Text("Selected:");
                ImGui::SameLine();
                ImGui::TextUnformatted(buffer[0] ? buffer : "(none)");

                ImGui::Separator();
                ImGui::InputText("Username", username_buffer, sizeof(username_buffer));
                ImGui::InputText("Password", pass_buffer, sizeof(pass_buffer), ImGuiInputTextFlags_Password);
                ImGui::InputText("Port", port_buffer, sizeof(port_buffer),
                                 ImGuiInputTextFlags_CharsDecimal | ImGuiInputTextFlags_CharsNoBlank);

                // Mode
                ImGui::Separator();
                ImGui::TextUnformatted("Connection Mode:");
                int m = (hostMode == ConnectionType::LOCALTUNNEL ? 0 : hostMode == ConnectionType::LOCAL ? 1
                                                                                                         : 2);
                if (ImGui::RadioButton("LocalTunnel (Recommended)", m == 0))
                    m = 0;
                ImGui::SameLine();
                if (ImGui::RadioButton("Local (LAN)", m == 1))
                    m = 1;
                ImGui::SameLine();
                if (ImGui::RadioButton("External (Internet)", m == 2))
                    m = 2;
                hostMode = (m == 0 ? ConnectionType::LOCALTUNNEL : m == 1 ? ConnectionType::LOCAL
                                                                          : ConnectionType::EXTERNAL);

                if (hostMode == ConnectionType::LOCALTUNNEL)
                {
                    ImGui::TextDisabled("LocalTunnel: URL appears after server starts.\n"
                                        "Copy connection string from Network Center.");
                    tryUpnp = false;
                }
                else if (hostMode == ConnectionType::LOCAL)
                {
                    ImGui::TextDisabled("Local (LAN): for players on the same network.");
                    tryUpnp = false;
                }
                else
                {
                    ImGui::TextDisabled("External: requires port forward (UPnP/manual) and may not work.");
                    ImGui::Checkbox("Try UPnP (auto port forward)", &tryUpnp);
                }

                ImGui::Separator();

                const bool valid = buffer[0] != '\0' && port_buffer[0] != '\0';
                if (ImGui::Button("Load & Host") && valid)
                {
                    // strip ".runic" â†’ game_table_name
                    std::string file = buffer;
                    std::string name = file;
                    const std::string suffix = ".runic";
                    if (name.size() >= suffix.size() &&
                        name.compare(name.size() - suffix.size(), suffix.size(), suffix) == 0)
                    {
                        name.resize(name.size() - suffix.size());
                    }
                    game_table_name = name;

                    // close current + load
                    board_manager->closeBoard();
                    active_game_table = flecs::entity(); //Clean before load from file
                    network_manager->closeServer();

                    std::filesystem::path game_table_file_path =
                        PathManager::getRootDirectory() / "GameTables" / game_table_name / file;
                    loadGameTable(game_table_file_path);

                    // start network
                    auto my_unique = identity_manager->myUniqueId();
                    identity_manager->setMyIdentity(my_unique, username_buffer);
                    network_manager->setNetworkPassword(pass_buffer);
                    const unsigned p = static_cast<unsigned>(atoi(port_buffer));
                    network_manager->startServer(hostMode, static_cast<unsigned short>(p), tryUpnp);

                    // cleanup
                    memset(buffer, 0, sizeof(buffer));
                    memset(username_buffer, 0, sizeof(username_buffer));
                    memset(pass_buffer, 0, sizeof(pass_buffer));
                    //memset(port_buffer, 0, sizeof(port_buffer));
                    tryUpnp = false;

                    ImGui::CloseCurrentPopup();
                }

                ImGui::SameLine();
                if (ImGui::Button("Cancel"))
                {
                    ImGui::CloseCurrentPopup();
                }

                ImGui::EndChild();
                ImGui::EndTabItem();
            }

            ImGui::EndTabBar();
        }

        // Note: Share/copyable connection strings live in Network Center (as you prefer)
        ImGui::EndPopup();
    }
}
void GameTableManager::manageGameTablesPopUp()
{
    namespace fs = std::filesystem;
    if (!ImGui::BeginPopupModal("Manage GameTables", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        return;

    // --- local UI state ---
    static bool refreshFS = true;
    static std::vector<std::string> tables;  // folder names under GameTables/
    static std::vector<std::string> boards;  // *.runic in GameTables/<table>/Boards
    static std::string selectedTable;        // folder name
    static std::string pendingBoardToRename; // filename (with .runic)
    static std::string pendingBoardToDelete; // filename (with .runic)
    static char tableRenameBuf[128] = {0};
    static char boardRenameBuf[128] = {0};

    const fs::path root = PathManager::getGameTablesPath();

    // inline "reload" blocks so we don't create helpers
    if (refreshFS)
    {
        tables.clear();
        if (fs::exists(root))
        {
            for (auto& e : fs::directory_iterator(root))
                if (e.is_directory())
                    tables.emplace_back(e.path().filename().string());
            std::sort(tables.begin(), tables.end());
        }
        // ensure selection is valid
        if (!selectedTable.empty() && !fs::exists(root / selectedTable))
            selectedTable.clear();

        boards.clear();
        if (!selectedTable.empty())
        {
            const fs::path boardsDir = root / selectedTable / "Boards";
            if (fs::exists(boardsDir))
            {
                for (auto& e : fs::directory_iterator(boardsDir))
                    if (e.is_regular_file() && e.path().extension() == ".runic")
                        boards.emplace_back(e.path().filename().string());
                std::sort(boards.begin(), boards.end());
            }
        }
        refreshFS = false;
    }

    const float leftW = 260.f;

    // LEFT panel: tables
    ImGui::BeginChild("tables-left", ImVec2(leftW, 420), true);
    ImGui::TextUnformatted("GameTables");
    ImGui::Separator();

    for (const auto& t : tables)
    {
        const bool sel = (t == selectedTable);
        if (ImGui::Selectable(t.c_str(), sel))
        {
            selectedTable = t;
            std::snprintf(tableRenameBuf, IM_ARRAYSIZE(tableRenameBuf), "%s", t.c_str());
            // refresh boards for this selection
            refreshFS = true;
        }
    }

    ImGui::Separator();
    ImGui::TextUnformatted("Selected Table:");
    ImGui::SameLine();
    ImGui::TextColored(ImVec4(0.8f, 0.9f, 1.f, 1.f), "%s", selectedTable.empty() ? "(none)" : selectedTable.c_str());

    ImGui::BeginDisabled(selectedTable.empty());
    if (ImGui::Button("Delete Table"))
        ImGui::OpenPopup("DeleteTable");
    ImGui::EndDisabled();

    // Delete table popup
    if (ImGui::BeginPopupModal("DeleteTable", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::Text("Delete table '%s'?\nThis removes the folder permanently.", selectedTable.c_str());
        ImGui::Separator();
        if (ImGui::Button("Delete", ImVec2(120, 0)))
        {
            try
            {
                const fs::path dir = root / selectedTable;
                if (fs::exists(dir))
                    fs::remove_all(dir);
                selectedTable.clear();
                refreshFS = true;
            }
            catch (...)
            { /* ignore */
            }
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0)))
            ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }

    ImGui::EndChild();

    ImGui::SameLine();

    // RIGHT panel: boards in selected table
    ImGui::BeginChild("boards-right", ImVec2(540, 420), true);
    ImGui::TextUnformatted("Boards");
    ImGui::Separator();

    if (selectedTable.empty())
    {
        ImGui::TextDisabled("Select a table to see its boards.");
    }
    else
    {
        for (const auto& f : boards)
        {
            ImGui::PushID(f.c_str());
            ImGui::TextUnformatted(f.c_str());
            ImGui::SameLine();

            ImGui::SameLine();
            if (ImGui::Button("Delete"))
            {
                pendingBoardToDelete = f;
                ImGui::OpenPopup("DeleteBoard");
            }

            // Delete board popup
            if (ImGui::BeginPopupModal("DeleteBoard", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
            {
                ImGui::Text("Delete board '%s'?", pendingBoardToDelete.c_str());
                ImGui::Separator();
                if (ImGui::Button("Delete", ImVec2(120, 0)))
                {
                    try
                    {
                        const fs::path boardsDir = root / selectedTable / "Boards";
                        const fs::path p = boardsDir / pendingBoardToDelete;
                        if (fs::exists(p))
                            fs::remove(p);
                        refreshFS = true;
                    }
                    catch (...)
                    { /* ignore */
                    }
                    ImGui::CloseCurrentPopup();
                }
                ImGui::SameLine();
                if (ImGui::Button("Cancel", ImVec2(120, 0)))
                    ImGui::CloseCurrentPopup();
                ImGui::EndPopup();
            }

            ImGui::PopID();
        }
        if (boards.empty())
            ImGui::TextDisabled("(no boards in this table)");
    }

    ImGui::EndChild();

    ImGui::Separator();
    if (ImGui::Button("Close", ImVec2(120, 0)))
        ImGui::CloseCurrentPopup();

    ImGui::EndPopup();
}

void GameTableManager::renderUsernameChangePopup()
{
    // You already call OpenPopup("Change Username") elsewhere
    if (ImGui::BeginPopupModal("Change Username", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        static char usernameBuf[64] = {0};

        // Seed the input when the popup appears
        if (ImGui::IsWindowAppearing())
        {
            const std::string cur = network_manager->getMyUsername();
            std::snprintf(usernameBuf, sizeof(usernameBuf), "%s", cur.c_str());
        }

        ImGui::TextUnformatted("Username for this table:");
        ImGui::InputText("##uname", usernameBuf, (int)sizeof(usernameBuf));
        ImGui::Separator();

        const bool hasTable = active_game_table.is_alive() && active_game_table.has<Identifier>();
        ImGui::BeginDisabled(!hasTable);
        if (ImGui::Button("Apply", ImVec2(120, 0)))
        {
            if (hasTable)
            {
                const uint64_t tableId = active_game_table.get<Identifier>()->id;

                const std::string oldU = identity_manager->myUsername();
                const std::string newU = usernameBuf;
                if (!newU.empty() && newU != oldU)
                {
                    identity_manager->setMyIdentity(identity_manager->myUniqueId(), newU);

                    // 2) Broadcast UsernameUpdate over Game DC (carry uniqueId!)
                    //    If you currently have a binary builder, change it to write uniqueId instead of peerId.
                    std::vector<uint8_t> payload;
                    network_manager->buildUserNameUpdate(payload,
                                                         tableId,
                                                         /*userUniqueId*/ identity_manager->myUniqueId(),
                                                         /*oldUsername*/ oldU,
                                                         /*newUsername*/ newU,
                                                         /*reboundFlag*/ false);
                    network_manager->broadcastUserNameUpdate(payload);

                    // 3) Apply locally (by uniqueId) for instant UI
                    board_manager->onUsernameChanged(identity_manager->myUniqueId(), newU);
                    chat_manager->replaceUsernameForUnique(identity_manager->myUniqueId(), newU);
                }
            }
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndDisabled();

        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0)))
        {
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
}

// INFORMATIONAL POPUPS
// ======================= Guide (Mini Wiki) =======================
void GameTableManager::guidePopUp()
{
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

    if (ImGui::BeginPopupModal("Guide", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        // --- state ---
        static int section = 0;
        static const char* kSections[] = {
            "Getting Started",
            "Connecting to a Game",
            "Game Tables",
            "Boards",
            "Markers",
            "Fog of War",
            "Toolbar & Interface",
            "Networking & Security",
            "Known Issues",
            "Appendix"};

        // quick helper
        auto Para = [](const char* s)
        {
            ImGui::TextWrapped("%s", s);
            ImGui::Dummy(ImVec2(0, 4));
        };

        ImGui::TextUnformatted("RunicVTT Guide");
        ImGui::Separator();

        // main area (Left nav + Right content)
        ImVec2 full = ImVec2(860, 520);
        ImGui::BeginChild("GuideContent", full, true);

        // Left: nav
        ImGui::BeginChild("GuideNav", ImVec2(240, 0), true);
        for (int i = 0; i < IM_ARRAYSIZE(kSections); ++i)
        {
            if (ImGui::Selectable(kSections[i], section == i))
                section = i;
        }
        ImGui::EndChild();

        ImGui::SameLine();

        // Right: body (scroll)
        ImGui::BeginChild("GuideBody", ImVec2(0, 0), true, ImGuiWindowFlags_AlwaysVerticalScrollbar);

        switch (section)
        {
            case 0: // Getting Started
            {
                ImGui::SeparatorText("Overview");
                Para("RunicVTT is a virtual tabletop for sharing boards, markers, and fog of war in real-time across peers.");

                ImGui::SeparatorText("Basic Flow");
                Para("Create or load a Game Table -> Host or Join -> Add a Board -> Place Markers / Fog -> Play.");

                ImGui::SeparatorText("Requirements");
                Para("- Windows 10/11 recommended.\n"
                     "- Internet connection for WAN.\n"
                     "- Allow the app in your firewall/antivirus.\n"
                     "- Read/write access for assets (Boards/Markers folders).");

                ImGui::SeparatorText("Terminology");
                Para("- Game Table: a saved session containing chat and world state.\n"
                     "- Board: a map image displayed to all players.\n"
                     "- Marker: a token/object placed on a board.\n"
                     "- Fog: an overlay hiding/revealing areas.\n"
                     "- Peer: a connected client (GM or Player).");
                break;
            }

            case 1: // Connecting to a Game
            {
                ImGui::SeparatorText("Connection String");
                Para("A connection string identifies the host session. Example formats:\n"
                     "-  https://runic-<yourLocalIp>.loca.lt?PASSWORD\n"
                     "-  runic:<host>:<port>?PASSWORD");

                ImGui::SeparatorText("Hosting");
                Para("Choose a connection mode: LocalTunnel (public URL), Local (LAN), or External (WAN with port forwarding). "
                     "Start hosting and copy the connection string from the Network Center.");

                ImGui::SeparatorText("Joining");
                Para("Ask the host for a connection string and password, then use 'Connect to GameTable' and paste it.");

                ImGui::SeparatorText("Troubleshooting");
                Para("- If connection closes over WAN: ensure firewall allows the app, port was fowarded manually or via UPnP and the host is reachable. \n"
                     "- If connection closes over LAN: ensure firewall allows the app, and the moldem dont block local connections. \n"
                     "- If connection closes over LocalTunnel: ensure firewall allows the app, and the generated URL is in the correct format https://runic-<YOURLOCALIP>.loca.lt. if not in the formar generate new by hosting again \n"
                     "- Make sure you are connected to a Wifi or Ethernet moldem connection, 4G/5G mobile network arent supported due to their complex NAT protection. \n"
                     "- Corporate networks or strict NAT may require TURN/relay.");
                break;
            }

            case 2: // Game Tables
            {
                ImGui::SeparatorText("Create a Game Table");
                Para("Open 'Host GameTable' -> Create tab -> set name/username/password/port -> choose mode -> Host.");

                ImGui::SeparatorText("Load a Game Table");
                Para("Open 'Host GameTable' -> Load tab -> select a saved table -> set credentials/port -> Host.");

                ImGui::SeparatorText("Lifecycle");
                Para("Networking is tied to the Game Table. Closing it stops all connections. "
                     "Chat and boards are saved per table.");
                break;
            }

            case 3: // Boards
            {
                ImGui::SeparatorText("Create a Board");
                Para("Use 'Add Board' or board toolbar -> choose an image (PNG/JPG). The image is shared to peers.");

                ImGui::SeparatorText("Edit Board");
                Para("Adjust size/scale, toggle grid, panning/zoom. Visibility affects whether players see it fully.");

                ImGui::SeparatorText("Networking Notes");
                Para("Large images are chunked and sent reliably. Very large files transfer but take longer.");
                break;
            }

            case 4: // Markers
            {
                ImGui::SeparatorText("Create Markers");
                Para("Use the Marker directory to place tokens. Drag markers to the board from the Markers directory window.");

                ImGui::SeparatorText("Edit & Ownership");
                Para("Edit window lets the GM set: owner peer ID, allow-all-players move, and locked state. \n"
                     "Players can only move owned/unlocked markers; the GM can always move.");

                ImGui::SeparatorText("Movement");
                Para("Drag markers to move. Updates are broadcast at a limited rate to reduce spam.");
                break;
            }

            case 5: // Fog of War
            {
                ImGui::SeparatorText("Create Fog");
                Para("Use the Fog tool to add opaque overlays to hide areas from players.");

                ImGui::SeparatorText("Edit/Remove");
                Para("Move/resize fog areas or delete them. Fog updates are synchronized to all peers.");

                ImGui::SeparatorText("Authority");
                Para("Fog is GM-controlled; players do not send fog updates.");
                break;
            }

            case 6: // Toolbar & Interface
            {
                ImGui::SeparatorText("Toolbar Overview");
                Para("- Move Tool: pan map or drag owned markers.\n"
                     "- Fog Tool: create fog areas.\n"
                     "- Edit/Delete: open edit window or remove entities.\n"
                     "- Zoom/Pan: mouse wheel and drag (when panning)."
                     "- Grid: open grid window to configure it.\n"
                     "- Camera: open camera window to configure it.\n");

                ImGui::SeparatorText("Windows & Panels");
                Para("- Chat Window: General chat + dice roller (/roll).\n"
                     "- Edit Window: per-entity size/visibility/ownership.\n"
                     "- Grid Window: per-board grid cell size/offset/visibility/snap to grid.\n"
                     "- Camera Window: per-board camera zoom via button and sliders and reset.\n"
                     "- Host Window: create or load gametable with credentials and port, start network and sets active gametable.\n"
                     "- Connect Window: connect to hosted gametable, connection string and credential.\n"
                     "- Network Center: peers, connection strings, status.");

                //ImGui::SeparatorText("Hotkeys (examples)");
                //Para("Zoom: Mouse Wheel\nPan: Middle Drag\nEdit: Right-Click on entity");
                break;
            }

            case 7: // Networking & Security
            {
                ImGui::SeparatorText("How It Works");
                Para("Peer-to-peer data channels synchronize boards, markers, fog, and chat.");

                ImGui::SeparatorText("Firewall / Antivirus");
                Para("Allow the executable on first run. Some AVs may slow initial connection.");

                ImGui::SeparatorText("Quality & Reliability");
                Para("Images are sent on reliable channels.");
                break;
            }

            case 8: // Known Issues
            {
                ImGui::SeparatorText("Limitations");
                Para("- Very large images transfer slowly.\n"
                     "- Network Over the 4G/5G doesnt work due to NAT restrictions. \n"
                     "- Network do not work on Mobile Internet of any form(USB Anchoring, Wifi Hotspot, Ethernet and Bluetooth). ");

                ImGui::SeparatorText("Troubleshooting");
                Para("If desync occurs, rejoin the session or have the host re-broadcast state via Game Table snapshot.");

                ImGui::SeparatorText("Reporting Bugs");
                Para("Collect logs from the log window/folder and describe steps to reproduce.");
                break;
            }

            case 9: // Appendix
            {
                ImGui::SeparatorText("File Paths");
                Para("- GameTables folder: <root path>/GameTables/ \n"
                     "- Boards folder: <GameTableFolder>/<GameTableName>/Boards/ \n"
                     "- Maps folder: <root path>/Maps/ \n"
                     "- Marker folder: <root path>/Marker/.");

                ImGui::SeparatorText("Glossary");
                Para("- GM: Game Master (host/authority).\n"
                     "- Player: peer participant.\n"
                     "- Peer: a connected client.");

                break;
            }
        }

        ImGui::EndChild(); // GuideBody
        ImGui::EndChild(); // GuideContent

        ImGui::Separator();
        if (ImGui::Button("Close"))
            ImGui::CloseCurrentPopup();

        ImGui::EndPopup();
    }
}

// ======================= About =======================
void GameTableManager::aboutPopUp()
{
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

    if (ImGui::BeginPopupModal("About", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        const char* appName = "RunicVTT";
        const char* version = "0.0.1";
        const char* author = "Pedro Vicente dos Santos";
        const char* yearRange = "2025";
        const char* repoUrl = "https://github.com/PedroVicente98/RunicVTT";

        ImGui::Text("%s  v%s", appName, version);
        ImGui::Separator();

        ImGui::TextWrapped(
            "RunicVTT is a virtual tabletop focused on fast peer-to-peer play using WebRTC data channels. "
            "Itâ€™s built with C++ and integrates ImGui, GLFW, GLEW, and Flecs.");

        ImGui::NewLine();
        ImGui::Text("Author: %s", author);
        ImGui::Text("Year:   %s", yearRange);

        ImGui::NewLine();
        ImGui::TextUnformatted("GitHub:");
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.4f, 0.7f, 1.0f, 1.0f), "%s", repoUrl);
        ImGui::SameLine();
        if (ImGui::SmallButton("Copy URL"))
        {
            ImGui::SetClipboardText(repoUrl);
        }

        // If you later want a button to open the browser on Windows:
        // if (ImGui::SmallButton("Open in Browser")) {
        // #ifdef _WIN32
        //     ShellExecuteA(nullptr, "open", repoUrl, nullptr, nullptr, SW_SHOWNORMAL);
        // #endif
        // }

        ImGui::Separator();
        if (ImGui::Button("Close"))
            ImGui::CloseCurrentPopup();

        ImGui::EndPopup();
    }
}

void GameTableManager::render(VertexArray& va, IndexBuffer& ib, Shader& shader, Shader& grid_shader, Renderer& renderer)
{
    if (isGameTableActive())
    {
        chat_manager->render();
    }
    if (board_manager->isBoardActive())
    {
        if (board_manager->isEditWindowOpen())
        {
            board_manager->renderEditWindow();
        }
        else
        {
            board_manager->setShowEditWindow(false);
        }

        if (network_manager->getPeerRole() == Role::GAMEMASTER)
        {
            board_manager->marker_directory->renderDirectory();
        }
        board_manager->renderBoard(va, ib, shader, grid_shader, renderer);
    }
}
