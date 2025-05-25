#include "NetworkManager.h"



NetworkManager::NetworkManager(flecs::world ecs) : ecs(ecs){

}

NetworkManager::~NetworkManager()
{
}

void NetworkManager::startServer(unsigned short port, bool isClient)
{
}

bool NetworkManager::connectToPeer(const std::string& connectionString)
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
