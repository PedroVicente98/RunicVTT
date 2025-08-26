#include "NetworkManager.h"
//#include "UPnPManager.h"
#include "NetworkUtilities.h"

NetworkManager::NetworkManager(flecs::world ecs) : ecs(ecs), peer_role(Role::NONE){
	getLocalIPAddress();
	getExternalIPAddress();
	rtc::InitLogger(rtc::LogLevel::Debug);
}

NetworkManager::~NetworkManager()
{
}

void NetworkManager::startServer(std::string internal_ip_address, unsigned short port)
{
	signalingServer = std::make_unique<SignalingServer>();
	peer_role = Role::GAMEMASTER;

	auto local_tunnel_url = NetworkUtilities::startLocalTunnel("runic-" + internal_ip_address, port);
	//auto internalIP = UPnPManager::getLocalIPv4Address();
	/*if (!UPnPManager::addPortMapping(internal_ip_address, port, port, "TCP", "libdatachannel Signaling Server")) {
		bool open_port_foward_tip = true;
		ShowPortForwardingHelpPopup(&open_port_foward_tip);
	}*/
	signalingServer->start(port);

	signalingClient = std::make_unique<SignalingClient>();
	signalingClient->connect(local_tunnel_url, port);

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
	signalingClient = std::make_unique<SignalingClient>();
	std::string server_ip;
	unsigned short port;
	std::string password;
	parseConnectionString(connectionString, server_ip, port, password);
	auto response = signalingClient->connect(server_ip, port);

	return response;
}

bool NetworkManager::disconectFromPeers()
{
	return false;
}

void NetworkManager::connectToWebRTCPeer(const std::string& peerId)
{
}

void NetworkManager::receiveSignal(const std::string& peerId, const std::string& message)
{
}

void NetworkManager::sendSignalToPeer(const std::string& peerId, const std::string& message)
{
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

//-------------------------------------------------------------------------------
// In a real project, use a proper library for this.


std::string NetworkManager::base64_encode(const std::string& in) { /* ... implementation ... */ return in; }
std::string NetworkManager::base64_decode(const std::string& in) { /* ... implementation ... */ return in; }

std::string NetworkManager::createAndEncodeOffer(std::shared_ptr<rtc::PeerConnection> pc) {
	std::promise<std::string> sdpPromise;
	auto sdpFuture = sdpPromise.get_future();
	std::vector<rtc::Candidate> candidates;
	bool gatheringComplete = false;

	// Create a data channel before the offer
	auto dc = pc->createDataChannel("game-data");
	dc->onOpen([]() { std::cout << "GM: DataChannel is open!" << std::endl; });

	pc->onLocalDescription([&](rtc::Description description) {
		sdpPromise.set_value(std::string(description));
		});

	pc->onLocalCandidate([&](rtc::Candidate candidate) {
		candidates.push_back(candidate);
		});

	pc->onGatheringStateChange([&](rtc::PeerConnection::GatheringState state) {
		if (state == rtc::PeerConnection::GatheringState::Complete) {
			gatheringComplete = true;
		}
		});

	// Start the process
	pc->setLocalDescription();

	// Wait for the offer SDP to be generated
	std::string offerSdp = sdpFuture.get();

	// Wait until all ICE candidates have been gathered
	while (!gatheringComplete) {
		std::this_thread::sleep_for(std::chrono::milliseconds(50));
	}

	// Now, bundle everything into a JSON object
	nlohmann::json offer_bundle;
	offer_bundle["type"] = "offer";
	offer_bundle["sdp"] = offerSdp;

	nlohmann::json json_candidates = nlohmann::json::array();
	for (const auto& candidate : candidates) {
		json_candidates.push_back({
			{"candidate", std::string(candidate)},
			{"mid", candidate.mid()}
			});
	}
	offer_bundle["candidates"] = json_candidates;

	std::cout << "GM: Offer and all candidates generated. Ready to encode." << std::endl;
	return base64_encode(offer_bundle.dump());
}

std::string NetworkManager::decodeOfferAndCreateAnswer(const std::string& offerString, std::shared_ptr<rtc::PeerConnection> pc) {
	// Decode the string from the GM
	nlohmann::json offer_bundle = nlohmann::json::parse(base64_decode(offerString));

	std::string sdp = offer_bundle["sdp"];
	pc->setRemoteDescription(rtc::Description(sdp, "offer"));

	for (const auto& j_cand : offer_bundle["candidates"]) {
		pc->addRemoteCandidate(rtc::Candidate(j_cand["candidate"], j_cand["mid"]));
	}

	std::cout << "Player: Decoded offer and added candidates." << std::endl;

	pc->onDataChannel([](std::shared_ptr<rtc::DataChannel> dc) {
		std::cout << "Player: DataChannel received!" << std::endl;
		dc->onOpen([]() { std::cout << "Player: DataChannel is open!" << std::endl; });
		});

	// Now, do the same process as the GM to create an answer
	std::promise<std::string> sdpPromise;
	auto sdpFuture = sdpPromise.get_future();
	std::vector<rtc::Candidate> candidates;
	bool gatheringComplete = false;

	pc->onLocalDescription([&](rtc::Description description) {
		sdpPromise.set_value(std::string(description));
		});
	pc->onLocalCandidate([&](rtc::Candidate candidate) {
		candidates.push_back(candidate);
		});
	pc->onGatheringStateChange([&](rtc::PeerConnection::GatheringState state) {
		if (state == rtc::PeerConnection::GatheringState::Complete) {
			gatheringComplete = true;
		}
		});

	pc->setLocalDescription(); // Generate the answer

	std::string answerSdp = sdpFuture.get();
	while (!gatheringComplete) {
		std::this_thread::sleep_for(std::chrono::milliseconds(50));
	}

	nlohmann::json answer_bundle;
	answer_bundle["type"] = "answer";
	answer_bundle["sdp"] = answerSdp;
	nlohmann::json json_candidates = nlohmann::json::array();
	for (const auto& candidate : candidates) {
		json_candidates.push_back({
			{"candidate", std::string(candidate)},
			{"mid", candidate.mid()}
			});
	}
	answer_bundle["candidates"] = json_candidates;

	std::cout << "Player: Answer and all candidates generated. Returning encoded string." << std::endl;
	return base64_encode(answer_bundle.dump());
}

void NetworkManager::decodeAnswerAndFinalize(const std::string& answerString, std::shared_ptr<rtc::PeerConnection> pc) {
	nlohmann::json answer_bundle = nlohmann::json::parse(base64_decode(answerString));

	std::string sdp = answer_bundle["sdp"];
	pc->setRemoteDescription(rtc::Description(sdp, "answer"));

	for (const auto& j_cand : answer_bundle["candidates"]) {
		pc->addRemoteCandidate(rtc::Candidate(j_cand["candidate"], j_cand["mid"]));
	}

	std::cout << "GM: Finalized connection with player. Should be connected now!" << std::endl;
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