#include "NetworkManager.h"
#include "UPnPManager.h"


NetworkManager::NetworkManager(flecs::world ecs) : ecs(ecs), peer_role(Role::NONE){
	getLocalIPAddress();
	getExternalIPAddress();
}

NetworkManager::~NetworkManager()
{
}

void NetworkManager::startServer(std::string internal_ip_address, unsigned short port)
{
	signalingServer = std::make_unique<SignalingServer>();
	peer_role = Role::GAMEMASTER;
	auto internalIP = UPnPManager::getLocalIPv4Address();
	UPnPManager::addPortMapping(internal_ip_address, port, port, "TCP", "libdatachannel Signaling Server");
	signalingServer->start(port);
}

void NetworkManager::closeServer()
{
	peer_role = Role::NONE;
	signalingServer->stop();
}


bool NetworkManager::connectToPeer(const std::string& connectionString)
{
	signalingClient = std::make_unique<SignalingClient>();
	return false;
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
		auto local_ip = UPnPManager::getLocalIPv4Address();
		local_ip_address = local_ip;
		return local_ip;
	}
	return local_ip_address;
}

std::string NetworkManager::getExternalIPAddress() {
	if (external_ip_address == "") {
		auto external_ip = UPnPManager::getExternalIPv4Address();
		external_ip_address = external_ip;
		return external_ip;
	}
	return external_ip_address;
}

std::string NetworkManager::getNetworkInfo(bool external) {

	if (external) {
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