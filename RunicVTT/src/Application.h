#pragma once
#include "MainWindow.h"

class Application {
public:
	Application();
	~Application();

	int create();
	int run();

private:
	GLFWwindow* window_handle; 
};
