#include <GL/glew.h> 
#include <GLFW/glfw3.h>
#include <iostream>
#include "Application.h"

#ifdef _WIN32   
// windows code goes here.  
#elif __linux__    
// linux code goes here.  
#else     
#error Platform not supported
#endif

int main(void)
{
    Application application = Application();
    application.create();

    if (glewInit() != GLEW_OK) {
        std::cout << "GLEW FAILED";
    }

    application.run();

    return 0;
}
