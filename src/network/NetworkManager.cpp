#include "NetworkManager.h"
#include "UPnPManager.h"


NetworkManager::NetworkManager(flecs::world ecs) : ecs(ecs), peer_role(Role::PLAYER){
}

NetworkManager::~NetworkManager()
{
}

void NetworkManager::startServer(unsigned short port)
{
	signalingServer = std::make_unique<SignalingServer>();
	peer_role = Role::GAMEMASTER;
	auto internalIP = UPnPManager::getLocalIPv4Address();
	UPnPManager::addPortMapping(internalIP, port, port, "TCP", "libdatachannel Signaling Server");
	signalingServer->start(port);
}

void NetworkManager::closeServer()
{
	peer_role = Role::PLAYER;
	signalingServer->stop();
}


bool NetworkManager::connectToPeer(const std::string& connectionString)
{
	signalingClient = std::make_unique<SignalingClient>();
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
