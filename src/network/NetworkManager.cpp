#include "NetworkManager.h"
//#include "UPnPManager.h"
#include "NetworkUtilities.h"
#include "SignalingServer.h"
#include "SignalingClient.h"
#include "Message.h"

NetworkManager::NetworkManager(flecs::world ecs) : ecs(ecs), peer_role(Role::NONE)
{
	getLocalIPAddress();
	getExternalIPAddress();
	rtc::InitLogger(rtc::LogLevel::Verbose);
	NetworkUtilities::setupTLS();
}

void NetworkManager::setup() {
	signalingClient = std::make_shared<SignalingClient>(weak_from_this());
	signalingServer = std::make_shared<SignalingServer>(weak_from_this());
}

NetworkManager::~NetworkManager()
{
}

void NetworkManager::startServer(std::string internal_ip_address, unsigned short port)
{
	peer_role = Role::GAMEMASTER;

	auto local_tunnel_url = NetworkUtilities::startLocalTunnel("runic-" + internal_ip_address, port);
	//auto internalIP = UPnPManager::getLocalIPv4Address();
	/*if (!UPnPManager::addPortMapping(internal_ip_address, port, port, "TCP", "libdatachannel Signaling Server")) {
		bool open_port_foward_tip = true;
		ShowPortForwardingHelpPopup(&open_port_foward_tip);
	}*/
	signalingServer->start(port);
	auto status = signalingClient->connect(local_tunnel_url, port);
}

void NetworkManager::closeServer()
{
	if (signalingServer.get() != nullptr) {
		peer_role = Role::NONE;
		signalingServer->stop();
	}
	NetworkUtilities::stopLocalTunnel();
}



bool NetworkManager::connectToPeer(const std::string& connectionString)
{
	std::string server_ip;
	unsigned short port;
	std::string password;
	parseConnectionString(connectionString, server_ip, port, password);
	auto response = signalingClient->connect(server_ip, port);
	peer_role = Role::PLAYER;
	return response;
}

bool NetworkManager::disconectFromPeers()
{
	return false;
}

std::string NetworkManager::getLocalIPAddress() {
	if (local_ip_address == "") {
		auto local_ip = NetworkUtilities::getLocalIPv4Address();
		local_ip_address = local_ip;
		return local_ip;
	}
	return local_ip_address;
}

std::string NetworkManager::getExternalIPAddress() {
	if (external_ip_address == "") {
		//auto external_ip = UPnPManager::getExternalIPv4Address();
		std::string external_ip = NetworkUtilities::httpGet(L"loca.lt", L"/mytunnelpassword");
		external_ip_address = external_ip;
		return external_ip;
	}
	return external_ip_address;
}

std::string NetworkManager::getNetworkInfo(bool external) {

	if (!external) {
		auto ip_address = getLocalIPAddress();
		auto port = getPort();
		std::string network_info = "runic:" + ip_address + ":" + std::to_string(port) + "?" + getNetworkPassword();
		return network_info;
	}
	else {
		auto ip_address = getExternalIPAddress();
		auto port = getPort();
		std::string network_info = "runic:" + ip_address + ":" + std::to_string(port) + "?" + getNetworkPassword();
		return network_info;
	}
}
std::string NetworkManager::getLocalTunnelURL() {
	return NetworkUtilities::getLocalTunnelUrl();
}


void NetworkManager::parseConnectionString(std::string connection_string, std::string& server_ip, unsigned short& port, std::string& password) {
	std::regex rgx(R"(runic:([\d.]+):(\d+)\??(.*))");
	std::smatch match;
	// Parse the connection string using regex
	if (std::regex_match(connection_string, match, rgx)) {
		server_ip = match[1];             // Extract the server's Hamachi IP address
		port = std::stoi(match[2]);    // Extract the port
		password = match[3];              // Extract the optional password
	}
}

void NetworkManager::setNetworkPassword(const char* password) {
	strncpy(network_password, password, sizeof(network_password) - 1);
	network_password[sizeof(network_password) - 1] = '\0';
}


void NetworkManager::ShowPortForwardingHelpPopup(bool* p_open) {
	if (*p_open) {
		ImGui::OpenPopup("Port Forwarding Failed");
	}

	if (ImGui::BeginPopupModal("Port Forwarding Failed", p_open, ImGuiWindowFlags_AlwaysAutoResize)) {
		ImGui::Text("Automatic port forwarding failed. This is likely because your router does not\n"
			"support UPnP or it is disabled. You can still host by choosing one of the\n"
			"following three options:");

		ImGui::Separator();

		if (ImGui::BeginTabBar("MyTabBar")) {

			// Tab 1: Enable UPnP
			if (ImGui::BeginTabItem("Enable UPnP")) {
				ImGui::Text("This is the easiest solution and will allow automatic port forwarding to work.");
				ImGui::Spacing();
				ImGui::BulletText("Access your router's administration page. This is usually done by\n"
					"typing its IP address (e.g., 192.168.1.1) in your browser.");
				ImGui::BulletText("Log in with your router's credentials.");
				ImGui::BulletText("Look for a setting named 'UPnP' or 'Universal Plug and Play' and enable it.");
				ImGui::BulletText("Save the settings and restart your router if necessary.");
				ImGui::BulletText("Try hosting again. The application should now automatically forward the port.");
				ImGui::EndTabItem();
			}

			// Tab 2: Manual Port Forwarding
			if (ImGui::BeginTabItem("Manual Port Forwarding")) {
				ImGui::Text("This method always works but requires you to configure your router manually.");
				ImGui::Spacing();
				ImGui::BulletText("Find your local IP address. Open Command Prompt and type 'ipconfig' to find it\n"
					"(e.g., 192.168.1.100).");
				ImGui::BulletText("Access your router's administration page.");
				ImGui::BulletText("Look for a setting named 'Port Forwarding', 'Virtual Server', or 'NAT'.");
				ImGui::BulletText("Create a new rule with the following details:");
				ImGui::Indent();
				ImGui::BulletText("Protocol: TCP");
				ImGui::BulletText("Internal Port: [Your application's port, e.g., 8080]");
				ImGui::BulletText("External Port: [The same port]");
				ImGui::BulletText("Internal IP: [Your local IP from step 1]");
				ImGui::Unindent();
				ImGui::BulletText("Save the rule and try hosting again.");
				ImGui::EndTabItem();
			}

			// Tab 3: Use a VPN (Hamachi)
			if (ImGui::BeginTabItem("Use a VPN (Hamachi)")) {
				ImGui::Text("A VPN creates a virtual local network, bypassing the need for port forwarding.");
				ImGui::Spacing();
				ImGui::BulletText("Download and install a VPN client like Hamachi.");
				ImGui::BulletText("Create a new network within the VPN client.");
				ImGui::BulletText("Share the network name and password with your friends.");
				ImGui::BulletText("Instruct your friends to join your network.");
				ImGui::BulletText("On the host screen, you should be able to see your VPN IP address.\n"
					"Use this IP to host your session.");
				ImGui::EndTabItem();
			}

			ImGui::EndTabBar();
		}

		if (ImGui::Button("Close")) {
			*p_open = false;
			ImGui::CloseCurrentPopup();
		}

		ImGui::EndPopup();
	}
}

////OPERATIONS------------------------------------------------------------------

//CALLBACKS --------------------------------------------------------------------
// NetworkManager.cpp
std::shared_ptr<PeerLink> NetworkManager::ensurePeerLink(const std::string& peerId) {
	if (auto it = peers.find(peerId); it != peers.end()) return it->second;
	auto link = std::make_shared<PeerLink>(peerId, weak_from_this());
	peers.emplace(peerId, link);
	return link;
}

void NetworkManager::onPeerLocalDescription(const std::string& peerId, const rtc::Description& desc) {
	// Build signaling message and send via SignalingClient
	nlohmann::json j;
	const std::string sdp = std::string(desc);
	if (desc.type() == rtc::Description::Type::Offer) {
		j = msg::makeOffer("", peerId, sdp);
		
	}
	else if (desc.type() == rtc::Description::Type::Answer) {
		j = msg::makeAnswer("", peerId, sdp);
	}
	else {
		return;
	}
	if (!signalingClient) signalingClient->send(j.dump());
}

void NetworkManager::onPeerLocalCandidate(const std::string& peerId, const rtc::Candidate& cand) {
	auto j = msg::makeCandidate("", peerId, cand.candidate());
	if (signalingClient) signalingClient->send(j.dump());
}




//void onMessageFromPeer(const std::string& peerId, const std::string& message) {
//	nlohmann::json msg = nlohmann::json::parse(message);
//	std::string type = msg["type"];
//
//	if (type == "relay") {
//		std::string destinationId = msg["to"];
//		auto payload = msg["payload"];
//
//		// Find the data channel for the destination peer
//		auto it = peerDataChannels.find(destinationId);
//		if (it != peerDataChannels.end()) {
//			std::cout << "Relaying message from " << peerId << " to " << destinationId << std::endl;
//			// Forward the payload
//			it->second->sendMessage(payload.dump());
//		}
//	}
//	else {
//		// Handle normal game-state messages from this peer
//	}
//}





































//static std::vector<PeerConnectionState> offererPeers;
//static char answererOfferInput[2048] = "";
//static PeerConnectionState answererConnection;
//static std::vector<std::string> answererConnectedPeers;
//
//void renderNetworkOfferer() {
//	ImGui::Begin("Offerer / Gamemaster Control");
//
//	if (ImGui::Button("Add Peer Connection")) {
//		PeerConnectionState newState;
//		newState.id = offererPeers.empty() ? 0 : offererPeers.back().id + 1;
//		newState.pc = nullptr; // In real code: std::make_shared<rtc::PeerConnection>(config);
//
//		// Call createAndEncodeOffer asynchronously to avoid freezing the UI
//		newState.offerFuture = std::async(std::launch::async, createAndEncodeOffer, newState.pc);
//
//		offererPeers.push_back(std::move(newState));
//	}
//
//	ImGui::Separator();
//
//	for (auto& peer : offererPeers) {
//		ImGui::PushID(peer.id); // Use a unique ID for each peer's widgets
//
//		// Check if the async offer creation is complete
//		if (peer.offerFuture.valid() && peer.offerFuture.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
//			peer.offerString = peer.offerFuture.get();
//		}
//
//		// Display the generated offer string
//		ImGui::InputText("Offer to Copy", &peer.offerString[0], peer.offerString.size() + 1, ImGuiInputTextFlags_ReadOnly);
//		ImGui::SameLine();
//		if (ImGui::Button("Copy##Offer")) {
//			ImGui::SetClipboardText(peer.offerString.c_str());
//		}
//
//		// Input for the peer's answer
//		ImGui::InputText("Paste Answer Peer", peer.answerInputBuffer, sizeof(peer.answerInputBuffer));
//		ImGui::SameLine();
//
//		// Disable the connect button if already connected
//		if (peer.isConnected) ImGui::BeginDisabled();
//		if (ImGui::Button("Connect")) {
//			decodeAnswerAndFinalize(peer.answerInputBuffer, peer.pc);
//			peer.isConnected = true; // In real code, this would be set by a callback
//		}
//		if (peer.isConnected) ImGui::EndDisabled();
//
//		ImGui::Separator();
//		ImGui::PopID();
//	}
//
//	ImGui::End();
//}
//
//
//void renderNetworkAnswerer() {
//	ImGui::Begin("Answerer / Player Control");
//
//	ImGui::InputTextMultiline("Paste Offer", answererOfferInput, sizeof(answererOfferInput), ImVec2(-1, 80));
//
//	if (ImGui::Button("Create Answer")) {
//		answererConnection.pc = nullptr; // In real code: std::make_shared<rtc::PeerConnection>(config);
//		// Call asynchronously to avoid freezing
//		answererConnection.answerFuture = std::async(std::launch::async, decodeOfferAndCreateAnswer, std::string(answererOfferInput), answererConnection.pc);
//	}
//
//	// Check if the async answer creation is complete
//	if (answererConnection.answerFuture.valid() && answererConnection.answerFuture.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
//		answererConnection.answerString = answererConnection.answerFuture.get();
//		// Simulate connection success and add the GM to our list of peers
//		if (!answererConnection.isConnected) {
//			answererConnectedPeers.push_back("GM_Peer");
//			answererConnection.isConnected = true;
//		}
//	}
//
//	ImGui::InputTextMultiline("Answer to be Copied", &answererConnection.answerString[0], answererConnection.answerString.size() + 1, ImVec2(-1, 80), ImGuiInputTextFlags_ReadOnly);
//	ImGui::SameLine();
//	if (ImGui::Button("Copy##Answer")) {
//		ImGui::SetClipboardText(answererConnection.answerString.c_str());
//	}
//
//	ImGui::SeparatorText("Connected Peers");
//	if (ImGui::BeginListBox("##ConnectedPeers", ImVec2(-1, 0))) {
//		for (const auto& peerName : answererConnectedPeers) {
//			ImGui::Selectable(peerName.c_str());
//		}
//		ImGui::EndListBox();
//	}
//
//	ImGui::End();
//}