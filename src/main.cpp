#include "ApplicationHandler.h"
#include "PathManager.h"
#include "UPnPManager.h"

GLFWwindow* initializeOpenGLContext() {
    if (!glfwInit()) {
        std::cerr << "Falha ao inicializar o GLFW!" << std::endl;
        return nullptr;
    }

    // Cria a janela e inicializa o contexto OpenGL
    GLFWwindow* window = glfwCreateWindow(1280, 720, "Runic VTT", nullptr, nullptr);
    if (!window) {
        std::cerr << "Falha ao criar a janela GLFW!" << std::endl;
        glfwTerminate();
        return nullptr;
    }

    // Faz o contexto OpenGL atual para a janela
    glfwMakeContextCurrent(window);

    // Inicializa o GLEW (opcional, se estiver usando)
    if (glewInit() != GLEW_OK) {
        std::cerr << "Falha ao inicializar o GLEW!" << std::endl;
        return nullptr;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
    glfwWindowHint(GLFW_MAXIMIZED, GLFW_TRUE);

    // Retorna o ponteiro da janela
    return window;
}

// Função para definir o ícone da janela
void setWindowIcon(GLFWwindow* window, const char* iconPath) {
    // Carrega a imagem do ícone usando stb_image
    int width, height, channels;
    unsigned char* image = stbi_load(iconPath, &width, &height, &channels, 4);  // Força 4 canais (RGBA)
    if (!image) {
        std::cerr << "Erro: Não foi possível carregar o ícone: " << iconPath << std::endl;
        return;
    }

    // Cria o objeto GLFWimage e configura o ícone
    GLFWimage icon;
    icon.width = width;
    icon.height = height;
    icon.pixels = image;
    glfwSetWindowIcon(window, 1, &icon);  // Define o ícone

    // Libera a memória usada pela imagem
    stbi_image_free(image);
}

int main() {
//
//#ifndef _DEBUG
//    // Hide the console window in Release mode
//    ::ShowWindow(::GetConsoleWindow(), SW_HIDE);
//#endif
    // Inicializa o contexto OpenGL
    GLFWwindow* window = initializeOpenGLContext();
    if (!window) {
        return -1;  // Falha na inicialização
    }

    //auto pathManager = PathManager();
    PathManager::ensureDirectories();

    auto iconPath = PathManager::getResPath() / "RunicVTTIcon.png";
    setWindowIcon(window, iconPath.string().c_str());

    std::shared_ptr<DirectoryWindow> map_directory = std::make_shared<DirectoryWindow>(PathManager::getMapsPath().string(), "MapsDiretory");
    std::shared_ptr<DirectoryWindow> marker_directory = std::make_shared<DirectoryWindow>(PathManager::getMarkersPath().string(),"MarkersDirectory");
    ApplicationHandler app(window, map_directory, marker_directory);
    app.run();  // Executa o loop principal da aplicação

    glfwTerminate();  // Limpa os recursos do GLFW quando terminar
    return 0;
}
