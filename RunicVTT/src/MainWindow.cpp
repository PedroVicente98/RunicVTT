#include "MainWindow.h"

MainWindow::MainWindow(GLFWwindow* window_context)
{
	this->window_context = window_context;
	this->setup();
}

MainWindow::~MainWindow()
{
}


int MainWindow::setup()
{
	create_window("MainBoardWindow");

	create_window("MainBoardWindow", 1);

	return 0;
}


int MainWindow::create_window(std::string window_name, int window_type = 0) {
	if( window_type == 0)
	{
		
		Window window;
		WindowStruct board_window{ window, window_name };
		window_list.insert({ board_window.name, board_window });

		board_window.window.draw(window_type);

	}
	else if (window_type == 1) 
	{
		Window window;
		WindowStruct board_window{ window, window_name };
		window_list.insert({ board_window.name, board_window });

		board_window.window.draw(window_type);
	}

	return 0;
}


void MainWindow::draw()
{

}
void MainWindow::clear()
{
	GLCall(glClear(GL_COLOR_BUFFER_BIT));
}
