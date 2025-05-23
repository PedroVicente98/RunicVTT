#pragma once
#include <iostream>
#include <sstream>
#include "Message.h"
#include <cstdlib>  // For system()
#include "NetworkManager.h"
#include "flecs.h"

enum class Role { GAMEMASTER, PLAYER };

class NetworkManager {
public:
    NetworkManager(flecs::world ecs);
    ~NetworkManager();

};