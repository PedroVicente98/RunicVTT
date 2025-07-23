#include "GameTableManager.h"
#include "imgui_internal.h"
#include "Serializer.h"

GameTableManager::GameTableManager(flecs::world ecs, std::shared_ptr<DirectoryWindow> map_directory, std::shared_ptr<DirectoryWindow> marker_directory)
    : ecs(ecs), network_manager(ecs), map_directory(map_directory), board_manager(ecs, map_directory , marker_directory), chat()
{
    std::filesystem::path map_directory_path = PathManager::getMapsPath();
    map_directory->directoryName = "MapDiretory";
    map_directory->directoryPath = map_directory_path.string();
    map_directory->startMonitoring();
    map_directory->generateTextureIDs();
}


GameTableManager::~GameTableManager()
{

}

void GameTableManager::saveGameTable()
{
    createGameTableFile(active_game_table);
}

void GameTableManager::loadGameTable(std::filesystem::path game_table_file_path)
{
    std::ifstream inFile(game_table_file_path, std::ios::binary);
    if (inFile) {
        std::vector<unsigned char> buffer((std::istreambuf_iterator<char>(inFile)), std::istreambuf_iterator<char>());
        inFile.close();

        size_t offset = 0;
        active_game_table = Serializer::deserializeGameTableEntity(buffer, offset, ecs);
        ecs.defer_begin();
        try
        {   
            active_game_table.children([&](flecs::entity child) {
                if (child.has<Board>()) {
                    board_manager.setActiveBoard(child);
                    auto texture = child.get_mut<TextureComponent>();
                    auto board_image = map_directory->getImageByPath(texture->image_path);
                    texture->textureID = board_image.textureID;
                    texture->size = board_image.size;

                    child.children([&](flecs::entity grand_child) {
                        if (grand_child.has<MarkerComponent>()) {
                            auto grand_child_texture = grand_child.get_mut<TextureComponent>();
                            auto marker_image = board_manager.marker_directory->getImageByPath(grand_child_texture->image_path);
                            grand_child_texture->textureID = marker_image.textureID;
                            grand_child_texture->size = marker_image.size;
                        }
                    });
                }
            });
        }
        catch (const std::exception&)
        {
            std::cout << "ERROR LOADING IMAGES" << std::endl;
        }
        ecs.defer_end();
    }
    else {
        std::cerr << "Failed to load GameTable from " << game_table_file_path.string() << std::endl;
    }
}


bool GameTableManager::isBoardActive()
{
    return board_manager.isBoardActive();
}

bool GameTableManager::isGameTableActive()
{
	return active_game_table.is_valid();
}

//bool GameTableManager::isConnectionActive() {
//    return network_manager.isConnectionOpen();
//}
//
//bool GameTableManager::isConnected() const {
//    return network_manager.isConnected();
//}
//
//void GameTableManager::openConnection(unsigned short port) {
//    network_manager.startServer(port);
//}
//
//void GameTableManager::closeConnection() {
//    network_manager.stopServer();
//}
//
//
//void GameTableManager::processSentMessages() {
//    network_manager.processSentMessages();
//}
//void GameTableManager::processReceivedMessages() {
//    std::lock_guard<std::mutex> lock(network_manager.receivedQueueMutex);
//    // Process all messages in the queue
//    while (!network_manager.receivedQueue.empty()) {
//        const ReceivedMessage& receivedMessage = network_manager.receivedQueue.front();
//
//        switch (receivedMessage.type) {
//        case MessageType::ChatMessage:
//            // Process chat message, you can pass it to your chat system
//            std::cout << "[Chat] " << receivedMessage.user << ": " << receivedMessage.chatMessage << std::endl;
//            chat.addReceivedTextMessage(receivedMessage.user, receivedMessage.chatMessage);
//            break;
//
//        case MessageType::CreateMarker:
//            std::cout << "Creating marker from received message." << std::endl;
//            break;
//
//        default:
//            std::cerr << "Unknown message type received!" << std::endl;
//            break;
//        }
//        network_manager.receivedQueue.pop();
//    }
//}

// Função para configurar os callbacks do GLFW

void GameTableManager::setCameraFboDimensions(glm::vec2 fbo_dimensions) {
    board_manager.camera.setFboDimensions(fbo_dimensions);
};

void GameTableManager::handleInputs(glm::vec2 current_mouse_fbo_pos) {

    current_mouse_world_pos = board_manager.camera.screenToWorldPosition(current_mouse_fbo_pos);

    // Call individual handlers
    handleMouseButtonInputs();
    handleCursorInputs(); // This will primarily deal with dragging/panning
    handleScrollInputs();
}

void GameTableManager::handleMouseButtonInputs() {
    // Check if board is active first
    if (!isBoardActive()) return;

    // Left Mouse Button Press
    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        if (board_manager.getCurrentTool() == Tool::MOVE) {
            if (board_manager.isMouseOverMarker(current_mouse_world_pos)) { // Use world_pos
                board_manager.startMouseDrag(current_mouse_world_pos, false); // Drag Marker
            }
            else {
                board_manager.startMouseDrag(current_mouse_world_pos, true); // Pan Board
            }
        }
        if (board_manager.getCurrentTool() == Tool::FOG) {
            board_manager.startMouseDrag(current_mouse_world_pos, false); // Start fog drawing/erasing
        }
        if (board_manager.getCurrentTool() == Tool::SELECT) {
            auto entity = board_manager.getEntityAtMousePosition(current_mouse_world_pos); // Use world_pos
            if (entity.is_valid()) {
                board_manager.setShowEditWindow(true, entity);
            }
        }
    }

    // Left Mouse Button Release
    if (ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
        if (board_manager.isPanning() || board_manager.isDragginMarker()) {
            board_manager.endMouseDrag();
        }
        if (board_manager.isCreatingFog()) {
            board_manager.handleFogCreation(current_mouse_world_pos); // Use world_pos
            board_manager.endMouseDrag();
        }
    }
}

void GameTableManager::handleCursorInputs() {
    if (!isBoardActive()) return;

    // No need for global mouse position from GLFW anymore
    // current_mouse_pos is already updated to current_mouse_world_pos by handleInputs()

    if (board_manager.isPanning()) {
        board_manager.panBoard(current_mouse_world_pos); // Pan logic needs current world mouse position
    }

    if (board_manager.isDragginMarker()) {
        board_manager.handleMarkerDragging(current_mouse_world_pos); // Marker dragging logic needs current world mouse position
    }
    // Note: If Tool::FOG drawing involves continuous updates while dragging,
    // that logic would also go here, using current_mouse_world_pos.
    // Example: if (board_manager.isCreatingFog() && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) { ... }
}

void GameTableManager::handleScrollInputs() {
    if (!isBoardActive()) return;

    float mouse_wheel_delta = ImGui::GetIO().MouseWheel;

    if (mouse_wheel_delta != 0.0f) {
        float zoom_factor;
        if (mouse_wheel_delta > 0) {
            zoom_factor = 1.1f; // Zoom in by 10%
        }
        else {
            zoom_factor = 0.9f; // Zoom out by 10%
        }
        glm::vec2 mouse_pos_for_zoom = current_mouse_fbo_pos;

        board_manager.camera.zoom(zoom_factor, mouse_pos_for_zoom);
    }
}

//
//bool GameTableManager::isMouseInsideMapWindow() {
//    /*DEPRECATED*/
//    // Get the pointer to the MapWindow by name
//    ImGuiWindow* window = ImGui::FindWindowByName("MapWindow");
//    if (!window) return false;  // If window doesn't exist, return false
//
//    // Get the mouse position
//    ImVec2 mousePos = ImGui::GetMousePos();
//
//    // Get the window position and size
//    ImVec2 windowPos = window->Pos;  // Top-left corner
//    ImVec2 windowSize = window->Size;  // Size of the window
//
//    // Check if the mouse is inside the window boundaries
//    bool isInsideX = (mousePos.x >= windowPos.x && mousePos.x <= windowPos.x + windowSize.x);
//    bool isInsideY = (mousePos.y >= windowPos.y && mousePos.y <= windowPos.y + windowSize.y);
//
//    // Return true if both X and Y coordinates are inside the window
//    return isInsideX && isInsideY;
//}
//
//
//// Callback estático de botão do mouse
//void GameTableManager::mouseButtonCallback(GLFWwindow* window, int button, int action, int mods) {
//    // Recupera o ponteiro para a instância do GameTableManager
//    GameTableManager* game_table_manager = static_cast<GameTableManager*>(glfwGetWindowUserPointer(window));
//    if (!game_table_manager) return;
//
//    glm::vec2 mouse_pos = game_table_manager->current_mouse_pos;  // Pega a posição atual do mouse
//    if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS) {
//        if (game_table_manager->isBoardActive()) {
//            if (game_table_manager->isMouseInsideMapWindow()) {
//                if (game_table_manager->board_manager.getCurrentTool() == Tool::MOVE ) {
//		            if(game_table_manager->board_manager.isMouseOverMarker(mouse_pos)){
//			            game_table_manager->board_manager.startMouseDrag(mouse_pos, false);
//		            }else{
//                	    game_table_manager->board_manager.startMouseDrag(mouse_pos, true);
//		            }
//                }
//
//                if (game_table_manager->board_manager.getCurrentTool() == Tool::FOG) {
//                    game_table_manager->board_manager.startMouseDrag(mouse_pos, false);
//                }
//            
//                if (game_table_manager->board_manager.getCurrentTool() == Tool::SELECT) {
//                    auto entity = game_table_manager->board_manager.getEntityAtMousePosition(mouse_pos);
//                    if (entity. is_valid()) {
//                        game_table_manager->board_manager.setShowEditWindow(true, entity);
//                    }
//                }
//            }
//
//
//            //else if (game_table_manager->board_manager.getCurrentTool() == Tool::MARKER) {
//            //    game_table_manager->board_manager.handleMarkerSelection(mouse_pos);
//            //}
//        }
//    }
//
//    if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_RELEASE) 
//    {
//        if (game_table_manager->isBoardActive()) {
//
//            if (game_table_manager->board_manager.isPanning() || game_table_manager->board_manager.isDragginMarker()) {
//                game_table_manager->board_manager.endMouseDrag();
//            }
//            if (game_table_manager->board_manager.isCreatingFog()) {
//                auto imgui_mouse_pos = ImGui::GetMousePos();
//                game_table_manager->board_manager.handleFogCreation(glm::vec2(imgui_mouse_pos.x , imgui_mouse_pos.y));
//                game_table_manager->board_manager.endMouseDrag();
//            }
//        }
//
//    }
//
//}
//
//// Callback estático de movimentação do cursor
//void GameTableManager::cursorPositionCallback(GLFWwindow* window, double xpos, double ypos) {
//    // Recupera o ponteiro para a instância do GameTableManager
//    GameTableManager* game_table_manager = static_cast<GameTableManager*>(glfwGetWindowUserPointer(window));
//    if (!game_table_manager) return;
//    game_table_manager->current_mouse_pos = glm::vec2(xpos, ypos);  // Atualiza a posição do mouse
//
//    if (game_table_manager->isBoardActive()) {
//        if (game_table_manager->board_manager.isPanning()) {
//            game_table_manager->board_manager.panBoard(game_table_manager->current_mouse_pos);
//        }
//
//        if (game_table_manager->board_manager.isDragginMarker()) {
//            game_table_manager->board_manager.handleMarkerDragging(game_table_manager->current_mouse_pos);
//        }
//    }
//}
//
//// Callback estático de scroll (zoom)
//void GameTableManager::scrollCallback(GLFWwindow* window, double xoffset, double yoffset) {
//    // Recupera o ponteiro para a instância do GameTableManager
//    GameTableManager* game_table_manager = static_cast<GameTableManager*>(glfwGetWindowUserPointer(window));
//    if (!game_table_manager) return;
//
//    float zoom_factor = (yoffset > 0) ? 1.1f : 0.9f;  // Aumenta o zoom se o scroll for para cima, diminui se for para baixo
//    if (game_table_manager->isBoardActive()) {
//        game_table_manager->board_manager.zoomBoard(zoom_factor);  // Aplica o zoom no BoardManager
//    }
//}

// Save and Load Operations ------------------------------------------------------------------------------



void GameTableManager::createGameTableFile(flecs::entity game_table) {
    namespace fs = std::filesystem;
    auto game_tables_directory = PathManager::getGameTablesPath();
    auto active_game_table_folder = game_tables_directory / game_table_name;
    if (!fs::exists(active_game_table_folder) && !fs::is_directory(active_game_table_folder)) {
        std::filesystem::create_directory(active_game_table_folder);
    }

    std::vector<unsigned char> buffer;
    Serializer::serializeGameTableEntity(buffer, game_table, ecs);

    auto game_table_file = active_game_table_folder / (game_table_name + ".runic");
    std::ofstream file(game_table_file.string(), std::ios::binary);
    if (file.is_open()) {
        file.write(reinterpret_cast<const char*>(buffer.data()), buffer.size());
        file.close();
    }
    else {
        std::cerr << "Failed to open the file." << std::endl;
    }

}

//
std::vector<std::string> GameTableManager::listBoardFiles() {
    namespace fs = std::filesystem;
    auto game_tables_directory = PathManager::getGameTablesPath();
    auto active_game_table_folder = game_tables_directory / game_table_name;
    if (!fs::exists(active_game_table_folder) && !fs::is_directory(active_game_table_folder)) {
        std::filesystem::create_directory(active_game_table_folder);
    }
    auto game_table_boards_folder = active_game_table_folder / "Boards";

    if (!fs::exists(game_table_boards_folder) && !fs::is_directory(game_table_boards_folder)) {
        std::filesystem::create_directory(game_table_boards_folder);
    }
    std::vector<std::string> boards;
    for (const auto& entry : std::filesystem::directory_iterator(game_table_boards_folder)) {
        if (entry.is_regular_file()) {
            boards.emplace_back(entry.path().filename().string());
        }
    }

    return boards;

}

std::vector<std::string> GameTableManager::listGameTableFiles() {
    namespace fs = std::filesystem;
    auto game_tables_directory = PathManager::getGameTablesPath();

    // Verifica se o diretório "GameTables" existe
    if (!fs::exists(game_tables_directory) || !fs::is_directory(game_tables_directory)) {
        std::cerr << "GameTables directory does not exist!" << std::endl;
        return {};
    }
    std::vector<std::string> game_tables;
    // Itera sobre todos os diretórios dentro de "GameTables"
    for (const auto& folder : fs::directory_iterator(game_tables_directory)) {
        if (folder.is_directory()) {
            std::string folder_name = folder.path().filename().string();
            auto runic_file_path = folder.path() / (folder_name + ".runic");

            // Verifica se o arquivo "folder_name.runic" existe dentro da pasta
            if (fs::exists(runic_file_path) && fs::is_regular_file(runic_file_path)) {
                game_tables.emplace_back(runic_file_path.filename().string());  // Adiciona o nome do arquivo na lista
            }
        }
    }

    return game_tables;
}





// ----------------------------- GUI --------------------------------------------------------------------------------
void GameTableManager::createGameTablePopUp()
{   
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    if (ImGui::BeginPopupModal("CreateGameTable", 0, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::SetItemDefaultFocus();
        ImGui::InputText("GameTable Name", buffer, sizeof(buffer));
        game_table_name = buffer;

        ImGui::Separator();

     //   auto network_info = network_manager.getLocalIPAddress();
        ImGui::Text("Network Info");
   //     ImGui::Text(network_info.c_str());
        ImGui::InputText("Password", pass_buffer, sizeof(pass_buffer), ImGuiInputTextFlags_Password);
       // network_manager.setNetworkPassword(pass_buffer);
       
        ImGui::InputText("Port", port_buffer, sizeof(port_buffer), ImGuiInputTextFlags_CharsDecimal | ImGuiInputTextFlags_CharsNoBlank);

        if (ImGui::Button("Save") && strlen(buffer) > 0 && strlen(port_buffer) > 0)
        {

            board_manager.closeBoard();
            active_game_table = flecs::entity();
           // network_manager.stopServer();

            auto game_table = ecs.entity("GameTable").set(GameTable{ game_table_name });
            active_game_table = game_table;
            createGameTableFile(game_table);

            int port = atoi(port_buffer);
           // network_manager.startServer(port);

            memset(buffer, '\0', sizeof(buffer));
            memset(pass_buffer, '\0', sizeof(pass_buffer));
            memset(port_buffer, '\0', sizeof(port_buffer));

            ImGui::CloseCurrentPopup();
        }

        ImGui::SameLine();

        if (ImGui::Button("Close"))
        {
            ImGui::CloseCurrentPopup();
            memset(buffer, '\0', sizeof(buffer));
            memset(pass_buffer, '\0', sizeof(pass_buffer));
        }
        ImGui::EndPopup();
    }
}


void GameTableManager::createBoardPopUp() {
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    if (ImGui::BeginPopupModal("CreateBoard")) {
        ImGui::SetItemDefaultFocus();
        // Create a dockspace within the popup for the MapDirectory
        ImGuiID dockspace_id = ImGui::GetID("CreateBoardDockspace");

        // Create the dockspace
        if (ImGui::DockBuilderGetNode(dockspace_id) == 0) {
            ImGui::DockBuilderRemoveNode(dockspace_id);
            ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_DockSpace | ImGuiDockNodeFlags_PassthruCentralNode | ImGuiDockNodeFlags_NoUndocking);

            ImGui::DockBuilderDockWindow("MapDiretory", dockspace_id);
            ImGui::DockBuilderFinish(dockspace_id);
        }

        // Dividindo a janela em duas colunas
        ImGui::Columns(2, nullptr, false);

        // Coluna esquerda: formulário de criação do board
        ImGui::Text("Create a new board");

        // Campo para o nome do board
        ImGui::InputText("Board Name", buffer, sizeof(buffer));
        std::string board_name(buffer);

        ImGui::NewLine();

        // Pega a imagem selecionada do diretório de mapas
        DirectoryWindow::ImageData selectedImage = map_directory->getSelectedImage();

        if (!selectedImage.filename.empty()) {
            ImGui::Text("Selected Map: %s", selectedImage.filename.c_str());
            ImGui::Image((void*)(intptr_t)selectedImage.textureID, ImVec2(256, 256));  // Preview da imagem selecionada
        }
        else {
            ImGui::Text("No map selected. Please select a map.");
        }

        ImGui::NewLine();

        // Botão para salvar o board, habilitado apenas se houver nome e mapa selecionado
        if (ImGui::Button("Save") && !selectedImage.filename.empty() && strlen(buffer) > 0) {
            // Cria o board com o mapa e o nome inseridos
            auto board = board_manager.createBoard(board_name, selectedImage.filename, selectedImage.textureID, selectedImage.size);
            board.add(flecs::ChildOf, active_game_table);

            // Limpa a seleção após salvar
            map_directory->clearSelectedImage();
            memset(buffer, '\0', sizeof(buffer));

            // Fecha o popup após salvar
            ImGui::CloseCurrentPopup();
        }

        ImGui::SameLine();

        // Botão para fechar o popup
        if (ImGui::Button("Close")) {
            map_directory->clearSelectedImage();  // Limpa o caminho ao fechar
            ImGui::CloseCurrentPopup();
        }

        ImGui::NextColumn();

        ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_PassthruCentralNode);

        // Coluna direita: diretório de mapas para seleção de imagem
        map_directory->renderDirectory(true);  // Renderiza o diretório de mapas dentro da coluna direita

        ImGui::Columns(1);  // Volta para uma única coluna

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
            board_manager.closeBoard();
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

void GameTableManager::closeGameTablePopUp()
{
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    if (ImGui::BeginPopupModal("CloseGameTable", 0, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::Text("Close Current GameTable?? Any unsaved changes will be lost!!");
        if (ImGui::Button("Close"))
        {
            board_manager.closeBoard();
            active_game_table = flecs::entity();
           // network_manager.stopServer();
            chat.clearChat();
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
    if (ImGui::BeginPopupModal("ConnectToGameTable", 0, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::SetItemDefaultFocus();
        ImGui::InputText("Connection String", buffer, sizeof(buffer));
        ImGui::Separator();

        if (ImGui::Button("Connect") && strlen(buffer) > 0)
        {
            if (true)//network_manager.connectToPeer(buffer)) 
            {  
                memset(buffer, '\0', sizeof(buffer));

                ImGui::CloseCurrentPopup();
            }
            else
            {
                ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "Failed to Connect!!");
            }
            
        }

        ImGui::SameLine();

        if (ImGui::Button("Close"))
        {
            ImGui::CloseCurrentPopup();
            memset(buffer, '\0', sizeof(buffer));
        }
        ImGui::EndPopup();
    }
}

void GameTableManager::createNetworkPopUp() {
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

    if (ImGui::BeginPopupModal("CreateNetwork", 0, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::SetItemDefaultFocus();

        // Display local IP address (use NetworkManager to get IP)
        //std::string localIp = network_manager.getLocalIPAddress();  // New method to get IP address
        //ImGui::Text("Local IP Address: %s", localIp.c_str());

        // Input field for port
        ImGui::InputText("Port", port_buffer, sizeof(port_buffer));

        // Optional password field
        ImGui::InputText("Password (optional)", pass_buffer, sizeof(pass_buffer), ImGuiInputTextFlags_Password);

        ImGui::Separator();

        // Save button to start the network
        if (ImGui::Button("Start Network")) {
            unsigned short port = static_cast<unsigned short>(std::stoi(port_buffer));
            // Start the network with the given port and save the password
            //network_manager.setNetworkPassword(pass_buffer);
            //network_manager.startServer(port);
            ImGui::CloseCurrentPopup();

            // Clear buffers after saving
            memset(port_buffer, '\0', sizeof(port_buffer));
            memset(pass_buffer, '\0', sizeof(pass_buffer));
        }

        ImGui::SameLine();

        // Close button to exit the pop-up
        if (ImGui::Button("Close")) {
            ImGui::CloseCurrentPopup();
            memset(port_buffer, '\0', sizeof(port_buffer));
            memset(pass_buffer, '\0', sizeof(pass_buffer));
        }
        ImGui::EndPopup();
    }
}

void GameTableManager::closeNetworkPopUp() {
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    if (ImGui::BeginPopupModal("CloseNetwork", 0, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::Text("Close Current Conection?? Any unsaved changes will be lost!!");
        if (ImGui::Button("Close Connection"))
        {
            //network_manager.stopServer();
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

void GameTableManager::openNetworkInfoPopUp()
{
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    if (ImGui::BeginPopupModal("NetworkInfo", 0, ImGuiWindowFlags_AlwaysAutoResize))
    {

        //auto connection_string = network_manager.getNetworkInfo();
        //auto ip = network_manager.getLocalIPAddress();
        //auto port = network_manager.getPort();

        ImGui::Text("IP: ");
        ImGui::SameLine();
        //ImGui::Text(ip.c_str());
        
        ImGui::Text("PORT: ");
        ImGui::SameLine();
        //ImGui::Text(std::to_string(port).c_str());
        
        ImGui::Text("Connection String: ");
        ImGui::SameLine();
        //ImGui::Text(connection_string.c_str());
        ImGui::SameLine();
        if (ImGui::Button("Copy")) {
            //ImGui::SetClipboardText(connection_string.c_str());  // Copy the connection string to the clipboard
            ImGui::Text("Copied to clipboard!");
        }

        ImGui::NewLine();

        if (ImGui::Button("Close"))
        {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}




void GameTableManager::saveBoardPopUp() {
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    if (ImGui::BeginPopupModal("SaveBoard", 0, ImGuiWindowFlags_AlwaysAutoResize))
    {

        ImGui::Text("IP: ");
       


        ImGui::NewLine();
        if (ImGui::Button("Save")) {
            ImGui::CloseCurrentPopup();
        }

        ImGui::SameLine();

        if (ImGui::Button("Close"))
        {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

}

void GameTableManager::loadGameTablePopUp() {
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    if (ImGui::BeginPopupModal("LoadGameTable",0, ImGuiWindowFlags_AlwaysAutoResize))
    {

        ImGui::Text("Load GameTable: ");
        //auto network_info = network_manager.getLocalIPAddress();
        ImGui::Text("Network Info");
        //ImGui::Text(network_info.c_str());
        ImGui::InputText("Password", pass_buffer, sizeof(pass_buffer), ImGuiInputTextFlags_Password);
        //network_manager.setNetworkPassword(pass_buffer);

        ImGui::InputText("Port", port_buffer, sizeof(port_buffer), ImGuiInputTextFlags_CharsDecimal | ImGuiInputTextFlags_CharsNoBlank);

        ImGui::NewLine();
        ImGui::Separator();

        auto game_tables = listGameTableFiles();
        for (auto& game_table : game_tables) {
            if (ImGui::Button(game_table.c_str()) && strlen(port_buffer) > 0)
            {
                board_manager.closeBoard();
                active_game_table = flecs::entity();
                //network_manager.stopServer();

                std::string suffix = ".runic";
                size_t pos = game_table.rfind(suffix);  // Find the position of ".runic"
                if (pos != std::string::npos && pos == game_table.length() - suffix.length()) {
                    game_table_name = game_table.substr(0, pos);
                }
                else {
                    game_table_name = game_table;
                }

                std::filesystem::path game_table_file_path = PathManager::getRootDirectory() / "GameTables" / game_table_name / game_table;
                loadGameTable(game_table_file_path);

                int port = atoi(port_buffer);
                //network_manager.startServer(port);

                memset(buffer, '\0', sizeof(buffer));
                memset(pass_buffer, '\0', sizeof(pass_buffer));
                memset(port_buffer, '\0', sizeof(port_buffer));


                ImGui::CloseCurrentPopup();
            }
        }

        ImGui::NewLine();
        ImGui::Separator();

        if (ImGui::Button("Close"))
        {
            ImGui::CloseCurrentPopup();
            memset(buffer, '\0', sizeof(buffer));
            memset(pass_buffer, '\0', sizeof(pass_buffer));
        }
        ImGui::EndPopup();
    }
}


void GameTableManager::loadBoardPopUp() {
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    if (ImGui::BeginPopupModal("LoadBoard", 0, ImGuiWindowFlags_AlwaysAutoResize))
    {

        ImGui::Text("Load Board: ");

        ImGui::NewLine();
        ImGui::Separator();

        auto boards = listBoardFiles();
        for (auto& board : boards) {
            if (ImGui::Button(board.c_str())) 
            {
                std::filesystem::path board_file_path = PathManager::getRootDirectory() / "GameTables" / game_table_name / "Boards" / board;
                board_manager.loadActiveBoard(board_file_path.string());
                ImGui::CloseCurrentPopup();
            }
        }

        ImGui::NewLine();
        ImGui::Separator();

        if (ImGui::Button("Close"))
        {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}



void GameTableManager::render(VertexArray& va, IndexBuffer& ib, Shader& shader, Renderer& renderer)
{
    if (board_manager.isEditWindowOpen()) {
        board_manager.renderEditWindow();
    }
    else {
        board_manager.setShowEditWindow(false);
    }

    chat.renderChat();

    if (board_manager.isBoardActive()) {
        //if (network_manager.getPeerRole() == Role::GAMEMASTER) {
        board_manager.marker_directory->renderDirectory();
        //}
	    //board_manager.renderToolbar();
        board_manager.renderBoard(va, ib, shader, renderer);
    }
}
