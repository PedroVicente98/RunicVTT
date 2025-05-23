#include <GL/glew.h> 
#include <GLFW/glfw3.h>

#include "Renderer.h"
#include "Shader.h"
#include "VertexBuffer.h"
#include "IndexBuffer.h"
#include "VertexArray.h"
#include "VertexBufferLayout.h"
#include "Texture.h"

#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"


#ifdef _WIN32   
// windows code goes here.  
#elif __linux__    
// linux code goes here.  
#else     
#error Platform not supported
#endif

int main(void)
{
    GLFWwindow* window;

    /* Initialize the library */
    if (!glfwInit())
        return -1;
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_RESIZABLE, GL_TRUE);
    //glfwWindowHint(GLFW_MAXIMIZED, GLFW_TRUE);

    /* Create a windowed mode window and its OpenGL context */
    window = glfwCreateWindow(640, 480, "Hello World", NULL, NULL);
    if (!window)
    {
        glfwTerminate();
        return -1;
    }

    /* Make the window's context current */
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    if (glewInit() != GLEW_OK) {
        std::cout << "GLEW FAILED";
    }

    std::cout << glGetString(GL_VERSION) << std::endl;
    {//Escopo para finalizar OPenGL antes GlFW

        float positions[] = {
            -1.0f, -1.0f, 0.0f, 0.0f,//0
             1.0f, -1.0f, 1.0f, 0.0f,//1
             1.0f, 1.0f,  1.0f, 1.0f,//2
             -1.0f, 1.0f, 0.0f, 1.0f //3
        };

        unsigned int indices[] = {
            0 , 1 , 2,
            2 , 3 , 0
        };

        GLCall(glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA));
        GLCall(glEnable(GL_BLEND));

        VertexArray va;
        VertexBuffer vb(positions, 4/*number of vertexes*/ * 4/*floats per vertex*/ * sizeof(float));
        VertexBufferLayout layout;

        layout.Push<float>(2);
        layout.Push<float>(2);
        va.AddBuffer(vb, layout);

        IndexBuffer ib(indices, 6);

        glm::mat4 proj = glm::ortho(-1.0f, 1.0f, -1.0f, 1.0f, 1.0f, -1.0f);

        Shader shader("res/shaders/Basic.shader");
        shader.Bind();
        //shader.SetUniform4f("u_Color", 0.8f, 0.3f, 0.8f, 1.0f);
        shader.SetUniform1i("u_Texture", 0);
        shader.SetUniformMat4f("u_MVP", proj);

        //Texture texture("res/texture/PhandalinBattlemap2.png");
        Texture texture("C:\\Dev\\TCC\\RunicVTT\\RunicVTT\\res\\textures\\PhandalinBattlemap2.png");
        //Texture texture("\\res\\texture\\PhandalinBattlemap2.png");
        texture.Bind();

        va.Unbind();
        vb.Unbind();
        ib.Unbind();
        shader.Unbind();

        Renderer renderer;
        
        /* Loop until the user closes the window */
        while (!glfwWindowShouldClose(window))
        {
            /* Render here */
            GLCall(glClear(GL_COLOR_BUFFER_BIT));

            shader.Bind();

            renderer.Draw(va, ib, shader);

            /* Swap front and back buffers */
            glfwSwapBuffers(window);
            /* Poll for and process events */
            glfwPollEvents();
        }
    }

    glfwTerminate();
    return 0;
}
