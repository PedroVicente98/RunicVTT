#include "NetworkManager.h"
#include "NetworkUtilities.h"
#include "SignalingServer.h"
#include "SignalingClient.h"
#include "Message.h"
#include "UPnPManager.h"

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

void NetworkManager::startServer(ConnectionType mode, unsigned short port, bool tryUpnp) {
	peer_role = Role::GAMEMASTER;

	// Ensure server exists (your setup() likely already does this)
	if (!signalingServer) {
		signalingServer = std::make_shared<SignalingServer>(shared_from_this());
	}
	if (!signalingClient) {
		signalingClient = std::make_shared<SignalingClient>(shared_from_this());
	}

	// Start WS server
	signalingServer->start(port);
	setPort(port);

	const std::string localIp = getLocalIPAddress();
	const std::string externalIp = getExternalIPAddress();

	switch (mode) {
	case ConnectionType::LOCALTUNNEL: {
		// Start LocalTunnel (non-blocking thread)
		// Note: URL will be available shortly after start; user can grab it from Network Center.
		const auto ltUrl = NetworkUtilities::startLocalTunnel("runic-" + localIp, static_cast<int>(port));
		if (ltUrl.empty()) {
			break;
		}
		signalingClient->connectUrl(ltUrl);

		break;
	}
	case ConnectionType::LOCAL: {
		// LAN hosting: no localtunnel, no UPnP.
		// GM can connect to itself using loopback to unify code paths
		signalingClient->connect("127.0.0.1", port);
		break;
	}
	case ConnectionType::EXTERNAL: {
		// Optional UPnP mapping for inbound connections
		if (tryUpnp) {
			try {
				// If you have UPnPManager available, call it here
				 if (!UPnPManager::addPortMapping(localIp, port, port, "TCP", "RunicVTT")) {
				     // optionally show your help popup:
				      bool open = true; ShowPortForwardingHelpPopup(&open);
				 }
			}
			catch (...) {
				// swallow errors; user was warned that it might not work
			}
		}
		// GM connects to itself locally
		signalingClient->connect("127.0.0.1", port);
		break;
	}
	}
}

// Keep your old method (back-compat). Default it to LocalTunnel behavior.
void NetworkManager::startServer(std::string internal_ip_address, unsigned short port) {
	// Old default used LocalTunnel; keep that behavior
	startServer(ConnectionType::LOCALTUNNEL, port, /*tryUpnp=*/false);
}

//
//void NetworkManager::startServer(std::string internal_ip_address, unsigned short port)
//{
//	peer_role = Role::GAMEMASTER;
//
//	auto local_tunnel_url = NetworkUtilities::startLocalTunnel("runic-" + internal_ip_address, port);
//	//auto internalIP = UPnPManager::getLocalIPv4Address();
//	/*if (!UPnPManager::addPortMapping(internal_ip_address, port, port, "TCP", "libdatachannel Signaling Server")) {
//		bool open_port_foward_tip = true;
//		ShowPortForwardingHelpPopup(&open_port_foward_tip);
//	}*/
//	signalingServer->start(port);
//	auto status = signalingClient->connect(local_tunnel_url, port);
//}

void NetworkManager::closeServer()
{
	if (signalingServer.get() != nullptr) {
		peer_role = Role::NONE;
		signalingServer->stop();
	}
	NetworkUtilities::stopLocalTunnel();
}



//bool NetworkManager::connectToPeer(const std::string& connectionString)
//{
//	std::string server_ip;
//	unsigned short port;
//	std::string password;
//	parseConnectionString(connectionString, server_ip, port, password);
//	auto response = signalingClient->connect(server_ip, port);
//	peer_role = Role::PLAYER;
//	return response;
//}


bool NetworkManager::connectToPeer(const std::string& connectionString)
{
	std::string server; unsigned short port = 0; std::string password;
	parseConnectionString(connectionString, server, port, password);

	if (!password.empty()) setNetworkPassword(password.c_str());
	peer_role = Role::PLAYER;

	if (hasUrlScheme(server)) {
		return signalingClient->connectUrl(server); // NEW method (see below)
	}
	return signalingClient->connect(server, port); // your old method
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

std::string NetworkManager::getNetworkInfo(ConnectionType type) {
	const auto port = getPort();
	const auto pwd = getNetworkPassword();

	if (type == ConnectionType::LOCAL) { // LAN (192.168.x.y)
		const auto ip = getLocalIPAddress();
		return "runic:" + ip + ":" + std::to_string(port) + "?" + pwd;
	}
	else if (type == ConnectionType::EXTERNAL) { // public IP
		const auto ip = getExternalIPAddress();
		return "runic:" + ip + ":" + std::to_string(port) + "?" + pwd;
	}
	else if (type == ConnectionType::LOCALTUNNEL) {
		const auto url = getLocalTunnelURL(); // e.g., https://sub.loca.lt
		return url + "?" + pwd;    // no port needed (LT handles it)
	}
	return {};
}

std::string NetworkManager::getLocalTunnelURL() {
	return NetworkUtilities::getLocalTunnelUrl();
}


//void NetworkManager::parseConnectionString(std::string connection_string, std::string& server_ip, unsigned short& port, std::string& password) {
//	std::regex rgx(R"(runic:([\d.]+):(\d+)\??(.*))");
//	std::smatch match;
//	// Parse the connection string using regex
//	if (std::regex_match(connection_string, match, rgx)) {
//		server_ip = match[1];             // Extract the server's Hamachi IP address
//		port = std::stoi(match[2]);    // Extract the port
//		password = match[3];              // Extract the optional password
//	}
//}


void NetworkManager::parseConnectionString(std::string connection_string,
	std::string& server, unsigned short& port,
	std::string& password)
{
	server.clear(); password.clear(); port = 0;

	const std::string prefix = "runic:";
	if (connection_string.rfind(prefix, 0) == 0)
		connection_string.erase(0, prefix.size());

	// split pass
	std::string left = connection_string;
	if (auto q = connection_string.find('?'); q != std::string::npos) {
		left = connection_string.substr(0, q);
		password = connection_string.substr(q + 1);
	}

	if (hasUrlScheme(left)) {      // URL path
		server = left;
		return;
	}

	// host[:port]
	if (auto colon = left.find(':'); colon != std::string::npos) {
		server = left.substr(0, colon);
		try { port = static_cast<unsigned short>(std::stoi(left.substr(colon + 1))); }
		catch (...) { port = 0; }
	}
	else {
		server = left; // no port
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

		if (ImGui::BeginTabBar("PortOptions")) {

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
bool NetworkManager::removePeer(std::string peerId) {
	auto it = peers.find(peerId);
	if (it == peers.end()) return false;
	if (auto& link = it->second) {
		try {
			link->close(); // ensure pc/dc closed
		}
		catch (...) {
			// swallow — safe cleanup path
		}
	}
	peers.erase(it);
	return true;
}

void NetworkManager::disconnectAllPeers() {
	for (auto& [id, link] : peers) {
		if (link) {
			try { link->close(); }
			catch (...) {}
		}
	}
	peers.clear();
}

// Optional: remove peers that are no longer usable (Closed/Failed or nullptr)
std::size_t NetworkManager::removeDisconnectedPeers() {
	std::size_t removed = 0;
	for (auto it = peers.begin(); it != peers.end(); ) {
		const bool shouldRemove =
			!it->second ||
			it->second->isClosedOrFailed(); // helper on PeerLink below

		if (shouldRemove) {
			if (it->second) { try { it->second->close(); } catch (...) {} }
			it = peers.erase(it);
			++removed;
		}
		else {
			++it;
		}
	}
	return removed;
}

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
		j = msg::makeOffer("", peerId, sdp, myUsername_);
		
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


// NetworkManager.cpp
void NetworkManager::setMyIdentity(std::string myId, std::string username) {
	if (myId.empty()) {
		myUsername_ = std::move(username);
	}
	else {
		clientUsernames_[myId] = username;
		myUsername_ = std::move(username);
		myClientId_ = std::move(myId);
	}
}

void NetworkManager::upsertPeerIdentity(const std::string& id, const std::string& username) {
	peerUsernames_[id] = username;
	// If PeerLink already exists, update its display name for logs/UI:
	if (auto it = peers.find(id); it != peers.end() && it->second) {
		it->second->setDisplayName(username);
	}
}

std::string NetworkManager::displayNameFor(const std::string& id) const {
	if (auto it = peerUsernames_.find(id); it != peerUsernames_.end()) return it->second;
	if (auto it = clientUsernames_.find(id); it != clientUsernames_.end()) return it->second;
	return id; // fallback to raw id
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
