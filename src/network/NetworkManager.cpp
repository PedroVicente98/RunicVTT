#include "NetworkManager.h"
#include "NetworkUtilities.h"
#include "SignalingServer.h"
#include "SignalingClient.h"
#include "Message.h"
#include "UPnPManager.h"
#include "Serializer.h"

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
		const auto ltUrl = NetworkUtilities::startLocalTunnel("runic-" + localIp, static_cast<int>(port));
		if (ltUrl.empty()) {
			break;
		}
		signalingClient->connectUrl(ltUrl);

		break;
	}
	case ConnectionType::LOCAL: {
		signalingClient->connect("127.0.0.1", port);
		break;
	}
	case ConnectionType::EXTERNAL:{
		if (tryUpnp) {
			try {
				 if (!UPnPManager::addPortMapping(localIp, port, port, "TCP", "RunicVTT")) {
				      bool open = true; ShowPortForwardingHelpPopup(&open);
				 }
			}
			catch (...) {
			}
		}
		signalingClient->connect("127.0.0.1", port);
		break;
	}
	}
	pushStatusToast("Signalling Server Started!!", NetworkToast::Level::Good, 4);
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

bool NetworkManager::isConnected(){

	return false;
}

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
		j = msg::makeAnswer("", peerId, sdp, myUsername_);
	}
	else {
		return;
	}
	if (signalingClient) signalingClient->send(j.dump());
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

bool NetworkManager::disconectFromPeers() {
	// Close all peer links (don’t let PeerLink::close() call back into NM to erase)
	for (auto& [pid, link] : peers) {
		if (link) {
			try { link->close(); }
			catch (...) {}
		}
	}
	peers.clear();

	// Close signaling client (player’s WS)
	if (signalingClient) {
		signalingClient->close();
		signalingClient.reset();
	}

	// We’re no longer connected
	peer_role = Role::NONE;
	return true;
}

bool NetworkManager::disconnectAllPeers() {
	// 1) Broadcast a shutdown message (optional, but nice UX)
	if (signalingServer) {
		signalingServer->broadcastShutdown();     // 1) tell everyone
	}

	// 2) Close all peer links locally
	for (auto& [pid, link] : peers) {
		if (link) {
			try { link->close(); }
			catch (...) {}
		}
	}
	peers.clear();

	// 3) Disconnect all WS clients and stop the server
	if (signalingServer) {
		signalingServer->disconnectAllClients();
		try { signalingServer->stop(); }
		catch (...) {}
		signalingServer.reset();
	}

	// 4) If GM also had a self client (loopback to its own server), close it too
	if (signalingClient) {
		signalingClient->close();
		signalingClient.reset();
	}

	peer_role = Role::NONE;
	return true;
}


void NetworkManager::broadcastPeerDisconnect(const std::string& targetId) {
    if (!signalingClient) return; // GM might be loopbacked as a client; else send via server
    auto j = msg::makePeerDisconnect(targetId, /*broadcast*/true);
    signalingClient->send(j.dump());
}

void NetworkManager::pushStatusToast(const std::string& msg, NetworkToast::Level lvl, double durationSec) {
    const double now = ImGui::GetTime();
    toasts_.push_back(NetworkToast{ msg, now + durationSec, lvl });
    if (toasts_.size() > 8) toasts_.pop_front(); // keep a small backlog
}

void NetworkManager::pruneToasts(double now) {
    while (!toasts_.empty() && toasts_.front().expiresAt <= now) {
        toasts_.pop_front();
    }
}



void NetworkManager::queueMessage(OutboundMsg msg) {
	outbound_.push(std::move(msg));
}

bool NetworkManager::tryDequeueOutbound(OutboundMsg& msg) {
	return outbound_.try_pop(msg);
}


bool NetworkManager::sendMarkerCreate(const std::string& to, uint64_t markerId,	const std::vector<unsigned char>& img, const std::string& name)
{
	if (img.empty()) return false;

	// BEGIN: DCType::Intent_MarkerCreate
	{
		std::vector<unsigned char> b;
		b.push_back(static_cast<uint8_t>(msg::DCType::Intent_MarkerCreate));
		Serializer::serializeUInt64(b, markerId);
		Serializer::serializeString(b, name);
		Serializer::serializeUInt64(b, static_cast<uint64_t>(img.size()));
		queueMessage(OutboundMsg{ to, msg::dc::name::Game, std::move(b) });
	}

	// CHUNKS: DCType::Image
	uint64_t offset = 0;
	while (offset < img.size()) {
		const int n = static_cast<int>(std::min<size_t>(kChunk, img.size() - offset));
		std::vector<unsigned char> b;
		b.push_back(static_cast<uint8_t>(msg::DCType::Image));
		Serializer::serializeUInt64(b, markerId);
		Serializer::serializeUInt64(b, offset);
		Serializer::serializeInt(b, n);
		b.insert(b.end(), img.data() + offset, img.data() + offset + n);
		queueMessage(OutboundMsg{ to, msg::dc::name::Game, std::move(b) });
		offset += n;
	}

	// COMMIT: DCType::CommitMarker
	{
		std::vector<unsigned char> b;
		b.push_back(static_cast<uint8_t>(msg::DCType::CommitMarker));
		Serializer::serializeUInt64(b, markerId);
		queueMessage(OutboundMsg{ to, msg::dc::name::Game, std::move(b) });
	}

	return true;
}

bool NetworkManager::sendBoardCreate(const std::string& to,	uint64_t boardId, const std::vector<unsigned char>& img, const std::string& name)
{
	if (img.empty()) return false;

	// BEGIN: we use DCType::Snapshot_Board as “BoardCreateBegin”
	{
		std::vector<unsigned char> b;
		b.push_back(static_cast<uint8_t>(msg::DCType::Snapshot_Board));
		Serializer::serializeUInt64(b, boardId);
		Serializer::serializeString(b, name);
		Serializer::serializeUInt64(b, static_cast<uint64_t>(img.size()));
		queueMessage(OutboundMsg{ to, msg::dc::name::Game, std::move(b) });
	}

	// CHUNKS: DCType::Image (same as marker)
	uint64_t offset = 0;
	while (offset < img.size()) {
		const int n = static_cast<int>(std::min<size_t>(kChunk, img.size() - offset));
		std::vector<unsigned char> b;
		b.push_back(static_cast<uint8_t>(msg::DCType::Image));
		Serializer::serializeUInt64(b, boardId);
		Serializer::serializeUInt64(b, offset);
		Serializer::serializeInt(b, n);
		b.insert(b.end(), img.data() + offset, img.data() + offset + n);
		queueMessage(OutboundMsg{ to, msg::dc::name::Game, std::move(b) });
		offset += n;
	}

	// COMMIT: DCType::CommitBoard
	{
		std::vector<unsigned char> b;
		b.push_back(static_cast<uint8_t>(msg::DCType::CommitBoard));
		Serializer::serializeUInt64(b, boardId);
		queueMessage(OutboundMsg{ to, msg::dc::name::Game, std::move(b) });
	}

	return true;
}




// NetworkManager.cpp
void NetworkManager::onDcGameBinary(const std::string& from, const unsigned char* data, size_t len) {
	if (!data || len < 1) return;

	const auto type = static_cast<msg::DCType>(data[0]);

	// We use Serializer by operating on a temp view (excluding first byte)
	std::vector<unsigned char> v;
	v.reserve(len - 1);
	v.insert(v.end(), data + 1, data + len);
	size_t off = 0;

	switch (type) {
	case msg::DCType::Intent_MarkerCreate: {
		// [id u64][string name][total u64]
		const uint64_t id = Serializer::deserializeUInt64(v, off);
		const std::string name = Serializer::deserializeString(v, off);
		const uint64_t total = Serializer::deserializeUInt64(v, off);

		PendingMarker pm;
		pm.name = name;
		pm.total = total;
		pm.received = 0;
		pm.buf.resize(static_cast<size_t>(total));
		markersRx_[id] = std::move(pm);
		break;
	}

	case msg::DCType::Snapshot_Board: {
		// Used as "BoardCreateBegin"
		// [id u64][string name][total u64]
		const uint64_t id = Serializer::deserializeUInt64(v, off);
		const std::string name = Serializer::deserializeString(v, off);
		const uint64_t total = Serializer::deserializeUInt64(v, off);

		PendingBoard pb;
		pb.name = name;
		pb.total = total;
		pb.received = 0;
		pb.buf.resize(static_cast<size_t>(total));
		boardsRx_[id] = std::move(pb);
		break;
	}

	case msg::DCType::Image: {
		// Shared chunk format for both marker/board payloads
		// [id u64][offset u64][len int][data ...]
		const uint64_t id = Serializer::deserializeUInt64(v, off);
		const uint64_t offset = Serializer::deserializeUInt64(v, off);
		const int      n = Serializer::deserializeInt(v, off);
		if (n <= 0 || off + static_cast<size_t>(n) > v.size()) return;

		const unsigned char* src = v.data() + off;

		// Try marker first
		if (auto it = markersRx_.find(id); it != markersRx_.end()) {
			auto& pm = it->second;
			if (offset + static_cast<uint64_t>(n) <= pm.total) {
				std::memcpy(pm.buf.data() + offset, src, n);
				pm.received += static_cast<uint64_t>(n);
			}
			break;
		}

		// Then board
		if (auto it = boardsRx_.find(id); it != boardsRx_.end()) {
			auto& pb = it->second;
			if (offset + static_cast<uint64_t>(n) <= pb.total) {
				std::memcpy(pb.buf.data() + offset, src, n);
				pb.received += static_cast<uint64_t>(n);
			}
			break;
		}

		// Unknown id → ignore
		break;
	}

	case msg::DCType::CommitMarker: {
		// [id u64]
		const uint64_t id = Serializer::deserializeUInt64(v, off);
		auto it = markersRx_.find(id);
		if (it == markersRx_.end()) break;

		PendingMarker pm = std::move(it->second);
		markersRx_.erase(it);
		if (pm.received == pm.total) {
			//inboundMarkers_.push({ from, id, std::move(pm.name), std::move(pm.buf) });
		}
		else {
			std::cerr << "[Net] CommitMarker incomplete (" << pm.received << "/" << pm.total << ")\n";
		}
		break;
	}

	case msg::DCType::CommitBoard: {
		// [id u64]
		const uint64_t id = Serializer::deserializeUInt64(v, off);
		auto it = boardsRx_.find(id);
		if (it == boardsRx_.end()) break;

		PendingBoard pb = std::move(it->second);
		boardsRx_.erase(it);
		if (pb.received == pb.total) {
			//inboundBoards_.push({ from, id, std::move(pb.name), std::move(pb.buf) });
		}
		else {
			std::cerr << "[Net] CommitBoard incomplete (" << pb.received << "/" << pb.total << ")\n";
		}
		break;
	}

	default:
		// ignore others here
		break;
	}
}














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
