#pragma once
#include "ApplicationHandler.h"

class Application {
public:
	Application();
	~Application();

	int run();
	int create();

private:
	GLFWwindow* window_handle; 
};
