#include "ApplicationHandler.h"
#include "PathManager.h"
#include "UPnPManager.h"

GLFWwindow* initializeOpenGLContext()
{
    if (!glfwInit())
    {
        std::cerr << "Falha ao inicializar o GLFW!" << std::endl;
        return nullptr;
    }

    // Cria a janela e inicializa o contexto OpenGL
    GLFWwindow* window = glfwCreateWindow(1280, 720, "Runic VTT", nullptr, nullptr);
    if (!window)
    {
        std::cerr << "Falha ao criar a janela GLFW!" << std::endl;
        glfwTerminate();
        return nullptr;
    }

    // Faz o contexto OpenGL atual para a janela
    glfwMakeContextCurrent(window);

    // Inicializa o GLEW (opcional, se estiver usando)
    if (glewInit() != GLEW_OK)
    {
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

GLFWimage loadImage(const char* iconPath)
{
    int width, height, channels;
    unsigned char* image = stbi_load(iconPath, &width, &height, &channels, 4); // Força 4 canais (RGBA)
    if (!image)
    {
        std::cerr << "Erro: Não foi possível carregar o ícone: " << iconPath << std::endl;
    }

    // Cria o objeto GLFWimage e configura o ícone
    GLFWimage icon;
    icon.width = width;
    icon.height = height;
    icon.pixels = image;

    return icon;
}

// Função para definir o ícone da janela
void setWindowIcon(GLFWwindow* window, std::filesystem::path iconFolderPath)
{
    // Carrega a imagem do ícone usando stb_image
    auto icon16 = iconFolderPath / "RunicVTTIcon_16.png";
    auto icon32 = iconFolderPath / "RunicVTTIcon_32.png";
    auto icon64 = iconFolderPath / "RunicVTTIcon_64.png";
    auto icon256 = iconFolderPath / "RunicVTTIcon.png";

    GLFWimage icons[4];
    icons[0] = loadImage(icon16.string().c_str());
    icons[1] = loadImage(icon32.string().c_str());
    icons[2] = loadImage(icon64.string().c_str());
    icons[3] = loadImage(icon256.string().c_str());

    glfwSetWindowIcon(window, 4, icons); // Define o ícone

    // Libera a memória usada pela imagem
    stbi_image_free(icons[0].pixels);
    stbi_image_free(icons[1].pixels);
    stbi_image_free(icons[2].pixels);
    stbi_image_free(icons[3].pixels);
}

int main()
{
    GLFWwindow* window = initializeOpenGLContext();
    if (!window)
    {
        return -1; // Falha na inicialização
    }

    PathManager::ensureDirectories();

    auto iconFolderPath = PathManager::getResPath();
    setWindowIcon(window, iconFolderPath);

    std::shared_ptr<DirectoryWindow> map_directory = std::make_shared<DirectoryWindow>(PathManager::getMapsPath().string(), "MapsDiretory");
    std::shared_ptr<DirectoryWindow> marker_directory = std::make_shared<DirectoryWindow>(PathManager::getMarkersPath().string(), "MarkersDirectory");
    ApplicationHandler app(window, map_directory, marker_directory);
    app.run();

    return 0;
}
