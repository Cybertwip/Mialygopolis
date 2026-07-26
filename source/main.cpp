#include "app.h"

#include <filesystem>
#include <iostream>
#include <string>

#ifndef M3D_DEFAULT_ASSET_DIR
#define M3D_DEFAULT_ASSET_DIR "generated/assets"
#endif
#ifndef M3D_CHARACTER_ASSET_ROOT
#define M3D_CHARACTER_ASSET_ROOT "3rdparty/character-assets"
#endif

namespace {
void printHelp(const char* executable)
{
    std::cout << "Prototipo nativo de Mialygopolis 3D\n\nUso: " << executable << " [opciones]\n\n"
              << "  --asset-dir RUTA       Directorio generado de catálogo y rasgos\n"
              << "  --source-assets RUTA   Raíz de texturas y miniaturas de CharacterStudio\n"
              << "  --smoke-test           Validar catálogo, integración, animación y movimiento\n"
              << "  --customizer           Iniciar en el estudio completo de personajes\n"
              << "  --lobby                Iniciar directamente en el lobby\n"
              << "  --world                Iniciar directamente en el mundo 3D\n"
              << "  --greek-heaven         Usar Cielo Griego en lugar de Ciudad Romana\n"
              << "  --character N          Seleccionar inicialmente el personaje N\n"
              << "  --demo-movement        Avanzar y correr automáticamente para pruebas\n"
              << "  --server HOST:PUERTO   Servidor multijugador (127.0.0.1:7777)\n"
              << "  --name NOMBRE          Nombre visible en el lobby\n"
              << "  --token TOKEN          Token de administrador o moderador\n"
              << "  --offline              Ejecutar sin conexión al lobby\n"
              << "  --screenshot RUTA      Guardar una captura OpenGL oculta y salir\n"
              << "  --width N / --height N Tamaño inicial de la ventana\n"
              << "  --auto-exit SEGUNDOS   Salir automáticamente\n"
              << "  --help                 Mostrar esta ayuda\n";
}
bool valueAfter(int argc, char** argv, int& index, std::string& value)
{
    if (index + 1 >= argc) { std::cerr << "Falta un valor después de " << argv[index] << "\n"; return false; }
    value = argv[++index]; return true;
}
}

int main(int argc, char** argv)
{
    m3d::AppOptions options;
    options.assetDirectory = M3D_DEFAULT_ASSET_DIR;
    options.sourceAssetDirectory = M3D_CHARACTER_ASSET_ROOT;
    for (int i = 1; i < argc; ++i) {
        const std::string argument = argv[i]; std::string value;
        if (argument == "--help" || argument == "-h") { printHelp(argv[0]); return 0; }
        else if (argument == "--smoke-test") options.smokeTest = true;
        else if (argument == "--customizer") options.startInCustomizer = true;
        else if (argument == "--lobby") options.startInLobby = true;
        else if (argument == "--world") options.startInWorld = true;
        else if (argument == "--greek-heaven" || argument == "--green-heaven") options.selectedWorld = 1;
        else if (argument == "--demo-movement") options.demoMovement = true;
        else if (argument == "--offline") options.offline = true;
        else if (argument == "--server") { if (!valueAfter(argc, argv, i, value)) return 2; options.serverAddress = value; }
        else if (argument == "--name") { if (!valueAfter(argc, argv, i, value)) return 2; options.playerName = value; }
        else if (argument == "--token") { if (!valueAfter(argc, argv, i, value)) return 2; options.roleToken = value; }
        else if (argument == "--asset-dir") { if (!valueAfter(argc, argv, i, value)) return 2; options.assetDirectory = value; }
        else if (argument == "--source-assets") { if (!valueAfter(argc, argv, i, value)) return 2; options.sourceAssetDirectory = value; }
        else if (argument == "--screenshot") { if (!valueAfter(argc, argv, i, value)) return 2; options.screenshotPath = value; }
        else if (argument == "--character") { if (!valueAfter(argc, argv, i, value)) return 2; options.selectedCharacter = std::stoi(value); }
        else if (argument == "--width") { if (!valueAfter(argc, argv, i, value)) return 2; options.width = std::stoi(value); }
        else if (argument == "--height") { if (!valueAfter(argc, argv, i, value)) return 2; options.height = std::stoi(value); }
        else if (argument == "--auto-exit") { if (!valueAfter(argc, argv, i, value)) return 2; options.autoExitSeconds = std::stof(value); }
        else { std::cerr << "Opción desconocida: " << argument << "\n"; printHelp(argv[0]); return 2; }
    }
    if (options.width < 320 || options.height < 240) { std::cerr << "El tamaño de la ventana debe ser de al menos 320x240\n"; return 2; }
    m3d::Application application; return application.run(options);
}
