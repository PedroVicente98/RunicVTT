#include "ApplicationHandler.h"
#include "PathManager.h"
#include "UPnPManager.h"
#include "DebugConsole.h"
#include "Logger.h"
#include "FirewallUtils.h"

namespace ShutdownUtils
{
    inline void ArmDeadManKill(unsigned timeoutMs)
    {
        std::thread([timeoutMs]()
                    {
                        std::this_thread::sleep_for(std::chrono::milliseconds(timeoutMs));
                        TerminateProcess(GetCurrentProcess(), 0); // hard kill (OS reclaims everything)
                    })
            .detach();
    }
} // namespace ShutdownUtils

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

static std::string getSelfPath()
{
    wchar_t buf[MAX_PATH];
    GetModuleFileNameW(nullptr, buf, MAX_PATH);
    std::wstring ws(buf);
    return std::string(ws.begin(), ws.end());
}

int main()
{
    DebugConsole::bootstrapStdCapture();
    Logger::instance().setChannelCapacity(4000);
    auto runic_exe = getSelfPath();
    auto node_exe = PathManager::getNodeExePath().string();
    auto runic_firewall_rule_name = "RunicVTT Inbound TCP (Any)";
    auto node_firewall_rule_name = "RunicVTT LocalTunnel(Any TCP)";

    FirewallUtils::addInboundAnyTcpForExe(runic_firewall_rule_name, runic_exe, /*Private*/ false);
    FirewallUtils::addInboundAnyTcpForExe(node_firewall_rule_name, node_exe, false);
    //FirewallUtils::addInboundAnyUdpForExe("RunicVTT Inbound UDP (Any)", runic_exe, /*Private*/ false);

    GLFWwindow* window = initializeOpenGLContext();
    if (!window)
    {
        return -1; // Falha na inicialização
    }

    PathManager::ensureDirectories();

    auto iconFolderPath = PathManager::getResPath();
    setWindowIcon(window, iconFolderPath);

    // before CreateProcessA
    auto nodeModules = (PathManager::getExternalPath() / "node" / "node_modules").string();
    _putenv_s("NODE_PATH", nodeModules.c_str());
    if (const char* np = std::getenv("NODE_PATH"))
    {
        Logger::instance().log("localtunnel", Logger::Level::Success, std::string("Parent NODE_PATH=") + np);
    }
    else
    {
        Logger::instance().log("localtunnel", Logger::Level::Error, "Parent NODE_PATH is not set");
    }

    std::shared_ptr<DirectoryWindow> map_directory = std::make_shared<DirectoryWindow>(PathManager::getMapsPath().string(), "MapsDiretory");
    std::shared_ptr<DirectoryWindow> marker_directory = std::make_shared<DirectoryWindow>(PathManager::getMarkersPath().string(), "MarkersDirectory");
    ApplicationHandler app(window, map_directory, marker_directory);
    app.run();

    FirewallUtils::removeRuleElevated(runic_firewall_rule_name);
    FirewallUtils::removeRuleElevated(node_firewall_rule_name);

    ShutdownUtils::ArmDeadManKill(2000); // 2s grace
    return 0;
}
