#include "ApplicationHandler.h"

ApplicationHandler::ApplicationHandler(GLFWwindow* window_context)
	: active_game_table(nullptr)
{
	this->window_context = window_context;
	this->setup();
}

ApplicationHandler::~ApplicationHandler()
{
}


int ApplicationHandler::setup()
{ 
	return 0;
}


GameTable ApplicationHandler::createGameTable()
{
	GameTable game_table;
	game_table.setup();
	setActiveGameTable(&game_table);
	return game_table;
}

void ApplicationHandler::loadRecentGameTables() {
	//load gametables files, order by modified date
	GameTable game_table;
	recent_game_tables.insert({ game_table.game_table_id, game_table });
}

void ApplicationHandler::setActiveGameTable(GameTable* game_table)
{
	active_game_table = game_table;
}

void ApplicationHandler::handleInput()
{
}

void ApplicationHandler::render()
{
	bool show_demo_window = true;
	if (show_demo_window)
		ImGui::ShowDemoWindow(&show_demo_window);


	ImGui::Begin("TesteWIndow");

	ImDrawList* drawList = ImGui::GetWindowDrawList();
	ImVec2 window_size = ImGui::GetWindowSize();
	ImVec2 window_pos = ImGui::GetWindowPos();

	ImVec2 boardOffset(window_pos.x + 50, window_pos.y+ 50); // Example offset for the board
	int gridSize = 10; // Example grid size
	// Render grid lines
	for (float x = boardOffset.x; x < boardOffset.x + 400; x += gridSize) {
		drawList->AddLine(ImVec2(x, boardOffset.y), ImVec2(x, boardOffset.y + 400), IM_COL32(255, 255, 255, 255));
	}
	for (float y = boardOffset.y; y < boardOffset.y + 400; y += gridSize) {
		drawList->AddLine(ImVec2(boardOffset.x, y), ImVec2(boardOffset.x + 400, y), IM_COL32(255, 255, 255, 255));
	}

	// Render markers
	ImVec2 markerPos(window_pos.x +150, window_pos.y + 150); // Example marker position
	float markerRadius = 10.0f; // Example marker radius
	drawList->AddCircleFilled(markerPos, markerRadius, IM_COL32(255, 0, 0, 255)); // Example red circle marker

	ImGui::End();
}


void ApplicationHandler::clear()
{
	GLCall(glClear(GL_COLOR_BUFFER_BIT));
}




void ApplicationHandler::renderMainMenuBar()
{
	if (ImGui::BeginMainMenuBar()) {
		if (ImGui::BeginMenu("File")) {
			if (ImGui::MenuItem("New Game Table")) {
				m_ShowNewGamePopup = true;
			}
			if (ImGui::MenuItem("Load Game Table")) {
				m_ShowLoadGamePopup = true;
			}
			if (ImGui::MenuItem("Save Game Table")) {
				// Handle saving game table
			}
			if (ImGui::MenuItem("Open Recent Game Table")) {
				// Handle opening recent game table
			/*	if (game_tables.size() <= 0) {
					ImGui::MenuItem("No Game Tables");
				}
				else {
					for (auto& game_table : game_tables) {
						if (ImGui::MenuItem(game_table.second.game_table_name.c_str())) {
							setActiveGameTable(&game_table.second);
						}
					}
				}
				ImGui::EndMenu();*/
			}
			ImGui::EndMenu();
		}
		ImGui::EndMainMenuBar();
	}
}

void ApplicationHandler::renderLoadGamePopup() {
	if (m_ShowLoadGamePopup) {
		ImGui::OpenPopup("Load Game Table");
		if (ImGui::BeginPopupModal("Load Game Table", &m_ShowLoadGamePopup)) {
			ImGui::Text("Select a game table to load:");
			for (const auto& gameTable : recent_game_tables) {
				if (ImGui::Selectable(gameTable.second.game_table_name.c_str())) {
					// Handle loading game table
					m_ShowLoadGamePopup = false;
				}
			}

			ImGui::EndPopup();
		}
	}
}

void ApplicationHandler::renderNewGamePopup() {
	if (m_ShowNewGamePopup) {
		ImGui::OpenPopup("New Game Table");
		if (ImGui::BeginPopupModal("New Game Table", &m_ShowNewGamePopup)) {
			ImGui::Text("Enter new game table name:");
			ImGui::InputText("Name", (char*)&m_NewGameName, IM_ARRAYSIZE(&m_NewGameName));

			if (ImGui::Button("Create", ImVec2(120, 0))) {
				// Handle creating new game table
				m_ShowNewGamePopup = false;
			}

			ImGui::EndPopup();
		}
	}
}



//void ApplicationHandler::beginApplicationHandler() {
//	static ImGuiDockNodeFlags dockspace_flags = ImGuiDockNodeFlags_None;
//
//	ImGuiViewport* viewport = ImGui::GetMainViewport();
//	//std::cout << viewport->Size.x << " , " << viewport->Size.y << std::endl;
//	
//	ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoScrollbar
//		| ImGuiWindowFlags_NoTitleBar
//		| ImGuiWindowFlags_NoCollapse
//		| ImGuiWindowFlags_NoResize
//		| ImGuiWindowFlags_NoMove
//		| ImGuiWindowFlags_NoBringToFrontOnFocus
//		| ImGuiWindowFlags_NoFocusOnAppearing
//		| ImGuiWindowFlags_NoNavFocus
//		| ImGuiWindowFlags_NoBackground;
//	
//	ImVec2 size = ImGui::GetContentRegionAvail();
//	ImGui::SetNextWindowPos(viewport->Pos, ImGuiCond_FirstUseEver);
//	ImGui::SetNextWindowSize(size, ImGuiCond_FirstUseEver);
//	ImGui::SetNextWindowViewport(viewport->ID);
//	ImGui::Begin("ApplicationHandler", nullptr, window_flags);
//	
//	//// DockSpace
//	//ImGuiIO& io = ImGui::GetIO();
//	//if (io.ConfigFlags & ImGuiConfigFlags_DockingEnable) {
//	//	ImGuiID dockspace_id = ImGui::GetID("MainDockSpace");
//	//	ImGui::DockSpace(dockspace_id, size, dockspace_flags);
//	//}
//	//ImGui::DockSpaceOverViewport(viewport, ImGuiDockNodeFlags_None);
//
//}
//
//void ApplicationHandler::endApplicationHandler() {
//	ImGui::End();
//}
