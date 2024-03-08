#include<iostream>

#define GLEW_STATIC
#include"GL/glew.h"

#include"GLFW/glfw3.h"

static unsigned int compileShader(unsigned int type, const std::string& shader)
{
    unsigned int id = glCreateShader(type);
    const char* src = shader.c_str();
    glShaderSource(id, 1, &src, NULL);
    glCompileShader(id);

    int result;
    glGetShaderiv(id, GL_COMPILE_STATUS, &result);
    if (!result) {
        char log[512];
        glGetShaderInfoLog(id, 512, NULL, log);
        std::cout << "Compiling shader error:\n" << log << std::endl;
    }

    return id;
}

static unsigned int createShader(const std::string& vertexShader, const std::string& fragmentShader)
{
    unsigned int program = glCreateProgram();
    unsigned int vs = compileShader(GL_VERTEX_SHADER, vertexShader);
    unsigned int fs = compileShader(GL_FRAGMENT_SHADER, fragmentShader);

    glAttachShader(program, vs);
    glAttachShader(program, fs);
    glLinkProgram(program);

    int result;
    glGetProgramiv(program, GL_LINK_STATUS, &result);
    if (!result) {
        char log[512];
        glGetProgramInfoLog(program, 512, NULL, log);
        std::cout << "linking shader error:\n" << log << std::endl;
    }

    glValidateProgram(program);

    glDeleteShader(vs);
    glDeleteShader(fs);

    return program;
}

void keyCallBack(GLFWwindow* window, int key, int scancode, int action, int mode)
{
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
        glfwSetWindowShouldClose(window, GL_TRUE);
}

int main()
{
    int width = 0, height = 0;

    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_COMPAT_PROFILE);
    glfwWindowHint(GLFW_RESIZABLE, GL_FALSE);

    GLFWwindow* window = glfwCreateWindow(500, 500, "Test", nullptr, nullptr);

    glfwMakeContextCurrent(window);

    glewExperimental = GL_TRUE;
    glewInit();
    glfwGetFramebufferSize(window, &width, &height);
    glViewport(0, 0, width, height);

    glfwSetKeyCallback(window, keyCallBack);

    float triangle[] = {
        -0.5f, -0.5f,
         0.0f,  0.5f,
         0.5f, -0.5f
    };

    GLuint buffer = 0;
    glGenBuffers(1, &buffer);
    glBindBuffer(GL_ARRAY_BUFFER, buffer);
    glBufferData(GL_ARRAY_BUFFER, sizeof(triangle), triangle, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 2, 0);
    glEnableVertexAttribArray(0);

    std::string vertexShader = "#version 330 core\n"
        "layout (location = 0) in vec4 position;\n"
        "void main()\n"
        "{\n"
        "gl_Position = position;\n"
        "}\n";

    std::string fragmentShader = "#version 330 core\n"
        "out vec4 color;\n"
        "void main()\n"
        "{\n"
        "color = vec4(1.0f, 0.0f, 0.0f, 0.0f);\n"
        "}\n";

    unsigned int program = createShader(vertexShader, fragmentShader);
    glUseProgram(program);

    while (!glfwWindowShouldClose(window)) {

        glfwPollEvents();

        glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        glDrawArrays(GL_TRIANGLES, 0, 3);

        glfwSwapBuffers(window);
    }

    glfwTerminate();
    return 0;
}