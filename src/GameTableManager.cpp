#include "GameTableManager.h"
#include "imgui/imgui_internal.h"

GameTableManager::GameTableManager(flecs::world ecs, std::string shader_directory_path)
    : ecs(ecs), board_manager(ecs, shader_directory_path), map_directory(std::string(), std::string())
{
    std::filesystem::path base_path = std::filesystem::current_path();
    std::string map_directory_name = "MapDiretory";
    std::filesystem::path map_directory_path = base_path / "res" / "textures";
    map_directory.directoryName = map_directory_name;
    map_directory.directoryPath = map_directory_path.string();
    map_directory.startMonitoring();
    map_directory.generateTextureIDs();
}


GameTableManager::~GameTableManager()
{

}

void GameTableManager::saveGameTable()
{
}

void GameTableManager::loadGameTable()
{
}


bool GameTableManager::isBoardActive()
{
    return board_manager.isBoardActive();
}

bool GameTableManager::isGameTableActive()
{
	return active_game_table.is_valid();
}

bool GameTableManager::isConnectionActive() {
    return network_manager.isConnectionOpen();
}

void GameTableManager::openConnection(unsigned short port) {
    network_manager.startServer(port);
    std::cout << "Connection opened: " << network_manager.getNetworkInfo() << std::endl;
}

void GameTableManager::closeConnection() {
    network_manager.stopServer();
    std::cout << "Connection closed." << std::endl;
}


// Fun��o para configurar os callbacks do GLFW
void GameTableManager::setInputCallbacks(GLFWwindow* window) {
    glfwSetMouseButtonCallback(window, mouseButtonCallback);
    glfwSetCursorPosCallback(window, cursorPositionCallback);
    glfwSetScrollCallback(window, scrollCallback);
}

// Callback est�tico de bot�o do mouse
void GameTableManager::mouseButtonCallback(GLFWwindow* window, int button, int action, int mods) {
    // Recupera o ponteiro para a inst�ncia do GameTableManager
    GameTableManager* game_table_manager = static_cast<GameTableManager*>(glfwGetWindowUserPointer(window));
    // Certifique-se de que o ponteiro foi corretamente recuperado
    if (!game_table_manager) return;
    //std::cout << "MOUSE BUTTON CALLBACK: " << button << " | " << action << " | " << mods << std::endl;
    glm::vec2 mouse_pos = game_table_manager->current_mouse_pos;  // Pega a posi��o atual do mouse
    if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS) {
        if (game_table_manager->isBoardActive()) {
            game_table_manager->board_manager.startMouseDrag(mouse_pos);
            if (game_table_manager->board_manager.getCurrentTool() == Tool::FOG) {
                game_table_manager->board_manager.handleFogCreation(mouse_pos);
            }
            else if (game_table_manager->board_manager.getCurrentTool() == Tool::MARKER) {
                game_table_manager->board_manager.handleMarkerSelection(mouse_pos);
            }
        }
    }
}

// Callback est�tico de movimenta��o do cursor
void GameTableManager::cursorPositionCallback(GLFWwindow* window, double xpos, double ypos) {
    // Recupera o ponteiro para a inst�ncia do GameTableManager
    GameTableManager* game_table_manager = static_cast<GameTableManager*>(glfwGetWindowUserPointer(window));
    if (!game_table_manager) return;
    //std::cout << "CURSOR POSITION CALLBACK: " << xpos << " | " << ypos << std::endl;
    game_table_manager->current_mouse_pos = glm::vec2(xpos, ypos);  // Atualiza a posi��o do mouse
    if (game_table_manager->isBoardActive()) {
        if (game_table_manager->board_manager.getCurrentTool() == Tool::MOVE) {
            game_table_manager->board_manager.handleMarkerDragging(game_table_manager->current_mouse_pos);
        }
    }
}

// Callback est�tico de scroll (zoom)
void GameTableManager::scrollCallback(GLFWwindow* window, double xoffset, double yoffset) {
    // Recupera o ponteiro para a inst�ncia do GameTableManager
    GameTableManager* game_table_manager = static_cast<GameTableManager*>(glfwGetWindowUserPointer(window));
    if (!game_table_manager) return;

    //std::cout << "SCROLL CALLBACK: " << yoffset << std::endl;
    float zoom_factor = (yoffset > 0) ? 1.1f : 0.9f;  // Aumenta o zoom se o scroll for para cima, diminui se for para baixo
    if (game_table_manager->isBoardActive()) {
        game_table_manager->board_manager.zoomBoard(zoom_factor);  // Aplica o zoom no BoardManager
    }
}




// ----------------------------- GUI --------------------------------------------------------------------------------
void GameTableManager::createGameTablePopUp()
{   
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    if (ImGui::BeginPopupModal("CreateGameTable"))
    {
        ImGui::SetItemDefaultFocus();
        ImGui::InputText("GameTable Name", buffer, sizeof(buffer));
        game_table_name = buffer;

        ImGui::Separator();
        ImGui::InputText("Password", pass_buffer, sizeof(buffer), ImGuiInputTextFlags_Password);
        
        if (ImGui::Button("Save") && strlen(buffer) > 0)
        {
            auto game_table = ecs.entity("GameTable").set(GameTable{ game_table_name });
            active_game_table = game_table;
            ImGui::CloseCurrentPopup();

            memset(buffer, '\0', sizeof(buffer));
            memset(pass_buffer, '\0', sizeof(pass_buffer));
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

        // Coluna esquerda: formul�rio de cria��o do board
        ImGui::Text("Create a new board");

        // Campo para o nome do board
        ImGui::InputText("Board Name", buffer, sizeof(buffer));
        std::string board_name(buffer);

        ImGui::NewLine();

        // Pega a imagem selecionada do diret�rio de mapas
        DirectoryWindow::ImageData selectedImage = map_directory.getSelectedImage();

        if (!selectedImage.filename.empty()) {
            ImGui::Text("Selected Map: %s", selectedImage.filename.c_str());
            ImGui::Image((void*)(intptr_t)selectedImage.textureID, ImVec2(256, 256));  // Preview da imagem selecionada
        }
        else {
            ImGui::Text("No map selected. Please select a map.");
        }

        ImGui::NewLine();

        // Bot�o para salvar o board, habilitado apenas se houver nome e mapa selecionado
        if (ImGui::Button("Save") && !selectedImage.filename.empty() && strlen(buffer) > 0) {
            // Cria o board com o mapa e o nome inseridos
            auto board = board_manager.createBoard(board_name, selectedImage.filename, selectedImage.textureID, selectedImage.size);
            board.add(flecs::ChildOf, active_game_table);

            // Limpa a sele��o ap�s salvar
            map_directory.clearSelectedImage();
            memset(buffer, '\0', sizeof(buffer));

            // Fecha o popup ap�s salvar
            ImGui::CloseCurrentPopup();
        }

        ImGui::SameLine();

        // Bot�o para fechar o popup
        if (ImGui::Button("Close")) {
            map_directory.clearSelectedImage();  // Limpa o caminho ao fechar
            ImGui::CloseCurrentPopup();
        }

        ImGui::NextColumn();

        ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_PassthruCentralNode);

        // Coluna direita: diret�rio de mapas para sele��o de imagem
        map_directory.renderDirectory(true);  // Renderiza o diret�rio de mapas dentro da coluna direita

        ImGui::Columns(1);  // Volta para uma �nica coluna

        ImGui::EndPopup();
    }
}

void GameTableManager::closeBoardPopUp()
{
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    if (ImGui::BeginPopupModal("CloseBoard"))
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
    if (ImGui::BeginPopupModal("CloseGameTable"))
    {
        ImGui::Text("Close Current GameTable?? Any unsaved changes will be lost!!");
        if (ImGui::Button("Close"))
        {
            board_manager.closeBoard();
            active_game_table = flecs::entity();
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


void GameTableManager::createNetworkPopUp() {
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

    if (ImGui::BeginPopupModal("CreateNetwork")) {
        ImGui::SetItemDefaultFocus();

        // Display local IP address (use NetworkManager to get IP)
        std::string localIp = network_manager.getLocalIPAddress();  // New method to get IP address
        ImGui::Text("Local IP Address: %s", localIp.c_str());

        // Input field for port
        ImGui::InputText("Port", port_buffer, sizeof(port_buffer));

        // Optional password field
        ImGui::InputText("Password (optional)", pass_buffer, sizeof(pass_buffer), ImGuiInputTextFlags_Password);

        ImGui::Separator();

        // Save button to start the network
        if (ImGui::Button("Start Network")) {
            unsigned short port = static_cast<unsigned short>(std::stoi(port_buffer));
            // Start the network with the given port and save the password
            network_manager.startServer(port);
            memcpy(network_password, pass_buffer, sizeof(pass_buffer));
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
    if (ImGui::BeginPopupModal("CloseNetwork"))
    {
        ImGui::Text("Close Current Conection?? Any unsaved changes will be lost!!");
        if (ImGui::Button("Close Connection"))
        {
            network_manager.stopServer();
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

void GameTableManager::render(VertexArray& va, IndexBuffer& ib, Shader& shader, Renderer& renderer)
{
    if (board_manager.isBoardActive()) {
        board_manager.marker_directory.renderDirectory();
        board_manager.renderBoard(va, ib, shader, renderer);
    }
}