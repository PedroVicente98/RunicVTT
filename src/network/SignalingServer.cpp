#include "SignalingServer.h"



void SignalingServer::start(const std::string& host, unsigned short port)
{
}

void SignalingServer::stop()
{
}

void SignalingServer::send(const std::string& toPeerId, const std::string& message)
{
}

void SignalingServer::onMessage(std::function<void(const std::string& fromPeerId, const std::string& msg)>)
{
}

void SignalingServer::onClientConnected(std::function<void(const std::string&)>)
{
}
