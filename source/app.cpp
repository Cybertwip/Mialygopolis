#include "app.h"

#include "bindings.h"
#include "character_loadout.h"
#include "glad/glad.h"
#include "multiplayer_client.h"
#include "renderer.h"
#include "voxel_world.h"

#include "SDL.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

namespace m3d {
namespace {

struct SessionDeleter { void operator()(M3DSession* session) const { m3d_session_destroy(session); } };
using SessionPtr = std::unique_ptr<M3DSession, SessionDeleter>;

SessionPtr createSession(const AppOptions& options)
{
    return SessionPtr(m3d_session_create((options.assetDirectory / "catalog.m3c").string().c_str()));
}

void printSdlError(const char* action) { std::cerr << action << ": " << SDL_GetError() << "\n"; }

PlayerState playerState(const M3DSession* session)
{
    const auto raw = m3d_session_player(session);
    PlayerState result;
    result.position = {raw.x, raw.y, raw.z};
    result.yaw = raw.yaw;
    result.speed = raw.speed;
    result.walkPhase = raw.walk_phase;
    return result;
}

} // namespace

int Application::run(const AppOptions& options)
{
    return options.smokeTest ? runSmokeTest(options) : runInteractive(options);
}

int Application::runSmokeTest(const AppOptions& options)
{
    try {
        SessionPtr session = createSession(options);
        if (!session) {
            std::cerr << "Falló la inicialización del catálogo o la sesión: " << m3d_last_error() << "\n";
            return 1;
        }
        if (m3d_session_character_count(session.get()) < 20 || m3d_session_group_count(session.get()) < 7) {
            std::cerr << "La prueba falló: el catálogo de CharacterStudio está incompleto\n";
            return 1;
        }

        m3d_session_select_character(session.get(), 1);
        m3d_session_open_customizer(session.get());
        m3d_session_randomize(session.get());
        CharacterAssetCache cache;
        CharacterLoadout loadout;
        std::string error;
        if (!loadout.sync(session.get(), cache, options.assetDirectory, options.sourceAssetDirectory, false, error)) {
            std::cerr << "La prueba falló al ensamblar rasgos aleatorios: " << error << "\n";
            return 1;
        }
        std::cout << loadout.name() << " configuración aleatoria: " << loadout.vertexCount()
                  << " vértices, " << loadout.triangleCount() << " triángulos\n";

        m3d_session_select_group(session.get(), 0);
        m3d_session_select_option_relative(session.get(), 1);
        const auto firstSwapStart = std::chrono::steady_clock::now();
        if (!loadout.sync(session.get(), cache, options.assetDirectory, options.sourceAssetDirectory, false, error)) return 1;
        const float firstSwapMs = std::chrono::duration<float, std::milli>(std::chrono::steady_clock::now() - firstSwapStart).count();
        m3d_session_select_option_relative(session.get(), -1);
        const auto cachedSwapStart = std::chrono::steady_clock::now();
        if (!loadout.sync(session.get(), cache, options.assetDirectory, options.sourceAssetDirectory, false, error)) return 1;
        const float cachedSwapMs = std::chrono::duration<float, std::milli>(std::chrono::steady_clock::now() - cachedSwapStart).count();
        std::cout << "Rendimiento de personalización: primera carga " << firstSwapMs
                  << " ms; retorno desde caché " << cachedSwapMs << " ms; "
                  << cache.assetCount() << " mallas reutilizables.\n";

        VoxelWorld romanWorld;
        romanWorld.generate(WorldTheme::RomanCity);
        VoxelWorld greekWorld;
        greekWorld.generate(WorldTheme::GreekHeaven);
        if (romanWorld.glyphs().empty() || greekWorld.glyphs().empty()) {
            std::cerr << "La prueba falló: uno de los mundos vóxel no tiene glifos de renderizado\n";
            return 1;
        }

        m3d_session_enter_lobby(session.get());
        m3d_session_enter_world(session.get());
        // Forward follows camera yaw; positive strafe must land on screen-left at yaw zero.
        m3d_session_set_move(session.get(), 1.0f, 0.25f, 1, 0.0f);
        for (int i = 0; i < 120; ++i) m3d_session_update(session.get(), 1.0f / 60.0f);
        const auto moved = m3d_session_player(session.get());
        if (m3d_session_mode(session.get()) != 3 || moved.z < 0.5f || moved.x >= 0.0f || moved.walk_phase <= 0.0f) {
            std::cerr << "La prueba falló: el movimiento relativo a la cámara o la fase cinemática es incorrecta\n";
            return 1;
        }
        m3d_session_return_to_customizer(session.get());
        if (m3d_session_mode(session.get()) != 1) {
            std::cerr << "La prueba falló: el mundo no regresó al personalizador\n";
            return 1;
        }
        std::cout << "Prueba nativa superada. El movimiento relativo a la cámara llegó a ("
                  << moved.x << ", " << moved.y << ", " << moved.z
                  << "), fase de animación " << moved.walk_phase << ".\n";
        return 0;
    } catch (const std::exception& exception) {
        std::cerr << "Excepción en la prueba: " << exception.what() << "\n";
        return 1;
    }
}

int Application::runInteractive(const AppOptions& options)
{
    SessionPtr session = createSession(options);
    if (!session) {
        std::cerr << "Falló la inicialización del catálogo o la sesión: " << m3d_last_error() << "\n";
        return 1;
    }
    m3d_session_select_character(session.get(), options.selectedCharacter);
    if (options.startInCustomizer || options.startInLobby || options.startInWorld) m3d_session_open_customizer(session.get());
    if (options.startInLobby) m3d_session_enter_lobby(session.get());
    if (options.startInWorld) { m3d_session_enter_lobby(session.get()); m3d_session_enter_world(session.get()); }

    std::array<std::unique_ptr<VoxelWorld>, WorldThemeCount> worlds;
    int selectedWorld = std::clamp(options.selectedWorld, 0, WorldThemeCount - 1);
    int loadedWorld = selectedWorld;
    try {
        worlds[static_cast<std::size_t>(loadedWorld)] = std::make_unique<VoxelWorld>();
        worlds[static_cast<std::size_t>(loadedWorld)]->generate(static_cast<WorldTheme>(loadedWorld));
    } catch (const std::exception& exception) {
        std::cerr << "Falló la generación del mundo: " << exception.what() << "\n";
        return 1;
    }

    // Keep Cocoa/IME text services disabled during gameplay. On macOS this
    // prevents the accent/alternate-character popover from appearing while a
    // movement key is held. Text input is enabled explicitly only for chat.
    SDL_SetHint(SDL_HINT_IME_SHOW_UI, "0");
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) { printSdlError("Falló SDL_Init"); return 1; }
    SDL_StopTextInput();
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3); SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
#ifdef __APPLE__
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG);
#endif
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1); SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8); SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1);
    SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 4);
    Uint32 flags = SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI;
    if (!options.screenshotPath.empty()) flags |= SDL_WINDOW_HIDDEN;
    SDL_Window* window = SDL_CreateWindow("Mialygopolis 3D", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                          options.width, options.height, flags);
    if (!window) {
        SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 0); SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 0);
        window = SDL_CreateWindow("Mialygopolis 3D", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                  options.width, options.height, flags);
    }
    if (!window) { printSdlError("Falló SDL_CreateWindow"); SDL_Quit(); return 1; }
    SDL_GLContext context = SDL_GL_CreateContext(window);
    if (!context) { printSdlError("Falló SDL_GL_CreateContext"); SDL_DestroyWindow(window); SDL_Quit(); return 1; }
    if (!gladLoadGLLoader(reinterpret_cast<GLADloadproc>(SDL_GL_GetProcAddress))) {
        std::cerr << "No se pudieron cargar las funciones de OpenGL\n"; SDL_GL_DeleteContext(context); SDL_DestroyWindow(window); SDL_Quit(); return 1;
    }
    SDL_GL_SetSwapInterval(options.screenshotPath.empty() ? 1 : 0);
    std::cout << "OpenGL: " << reinterpret_cast<const char*>(glGetString(GL_VERSION)) << "\n";

    Renderer renderer;
    std::string error;
    if (!renderer.initialize(error) || !renderer.uploadWorld(*worlds[static_cast<std::size_t>(loadedWorld)], error)) {
        std::cerr << "Falló la inicialización del renderizador: " << error << "\n";
        renderer.shutdown(); SDL_GL_DeleteContext(context); SDL_DestroyWindow(window); SDL_Quit(); return 1;
    }
    CharacterAssetCache assetCache;
    CharacterLoadout loadout;
    if (!loadout.sync(session.get(), assetCache, options.assetDirectory, options.sourceAssetDirectory, true, error)) {
        std::cerr << "Falló el ensamblaje inicial del personaje: " << error << "\n";
        renderer.shutdown(); SDL_GL_DeleteContext(context); SDL_DestroyWindow(window); SDL_Quit(); return 1;
    }
    std::uint64_t loadedRevision = m3d_session_customization_revision(session.get());
    SessionPtr preloadSession = createSession(options);
    CharacterLoadout preloadLoadout;
    std::vector<int> preloadOrder;
    if (preloadSession) {
        const int count = m3d_session_character_count(session.get());
        const int selected = m3d_session_selected_character(session.get());
        for (int distance = 1; static_cast<int>(preloadOrder.size()) < count - 1; ++distance) {
            const int right = (selected + distance) % count;
            const int left = (selected - distance + count) % count;
            if (std::find(preloadOrder.begin(), preloadOrder.end(), right) == preloadOrder.end() && right != selected) preloadOrder.push_back(right);
            if (static_cast<int>(preloadOrder.size()) >= count - 1) break;
            if (std::find(preloadOrder.begin(), preloadOrder.end(), left) == preloadOrder.end() && left != selected) preloadOrder.push_back(left);
        }
    }
    std::size_t preloadCursor = 0;
    float preloadAccumulator = 0.0f;
    MultiplayerClient multiplayer;
    if (!options.offline) {
        const auto character = m3d_session_character(session.get(), m3d_session_selected_character(session.get()));
        const std::string playerName = options.playerName.empty() ? (character.name ? character.name : "JUGADOR") : options.playerName;
        multiplayer.connectTo(options.serverAddress, playerName, options.roleToken,
                              m3d_session_selected_character(session.get()), selectedWorld);
    }

    if (options.screenshotPath.empty()) {
        SDL_ShowWindow(window); SDL_RaiseWindow(window);
        std::cout << "Selector: Izquierda/Derecha y Enter. Personalizador: Arriba/Abajo categoría, Izquierda/Derecha modelo, "
                     "[/] variante, R aleatorio, Enter abre el lobby. En lobby o mundo, T abre el chat.\n";
    }

    bool running = true, rightMouseDown = false;
    float cameraYaw = 0.0f, cameraDistance = 4.6f;
    int renderedFrames = 0;
    const auto startTime = std::chrono::steady_clock::now();
    auto previousTime = startTime;

    bool chatActive = false;
    std::string chatInput;
    float networkAccumulator = 0.0f;

    while (running) {
        SDL_Event event{};
        while (SDL_PollEvent(&event)) {
            const int mode = m3d_session_mode(session.get());
            if (event.type == SDL_TEXTINPUT && chatActive) {
                if (chatInput.size() < 240) chatInput += event.text.text;
                continue;
            }
            if (event.type == SDL_KEYDOWN && chatActive) {
                if (event.key.keysym.sym == SDLK_BACKSPACE && !chatInput.empty()) {
                    do { chatInput.pop_back(); }
                    while (!chatInput.empty() && (static_cast<unsigned char>(chatInput.back()) & 0xC0) == 0x80);
                } else if (event.key.keysym.sym == SDLK_RETURN) {
                    if (!chatInput.empty()) multiplayer.sendChat(chatInput);
                    chatInput.clear();
                    chatActive = false;
                    SDL_StopTextInput();
                } else if (event.key.keysym.sym == SDLK_ESCAPE) {
                    chatInput.clear();
                    chatActive = false;
                    SDL_StopTextInput();
                }
                continue;
            }

            switch (event.type) {
            case SDL_QUIT:
                running = false;
                break;
            case SDL_KEYDOWN:
                if (event.key.repeat) break;
                if ((event.key.keysym.mod & KMOD_CTRL) && event.key.keysym.sym == SDLK_q) {
                    running = false;
                    break;
                }
                if ((mode == 2 || mode == 3) && event.key.keysym.sym == SDLK_t) {
                    chatActive = true;
                    chatInput.clear();
                    SDL_StartTextInput();
                    break;
                }
                if (mode == 0) {
                    if (event.key.keysym.sym == SDLK_LEFT || event.key.keysym.sym == SDLK_a) m3d_session_select_relative(session.get(), -1);
                    else if (event.key.keysym.sym == SDLK_RIGHT || event.key.keysym.sym == SDLK_d) m3d_session_select_relative(session.get(), 1);
                    else if (event.key.keysym.sym == SDLK_RETURN || event.key.keysym.sym == SDLK_SPACE) m3d_session_open_customizer(session.get());
                    else if (event.key.keysym.sym == SDLK_ESCAPE) running = false;
                } else if (mode == 1) {
                    if (event.key.keysym.sym == SDLK_UP || event.key.keysym.sym == SDLK_w) m3d_session_select_group_relative(session.get(), -1);
                    else if (event.key.keysym.sym == SDLK_DOWN || event.key.keysym.sym == SDLK_s) m3d_session_select_group_relative(session.get(), 1);
                    else if (event.key.keysym.sym == SDLK_LEFT || event.key.keysym.sym == SDLK_a) m3d_session_select_option_relative(session.get(), -1);
                    else if (event.key.keysym.sym == SDLK_RIGHT || event.key.keysym.sym == SDLK_d) m3d_session_select_option_relative(session.get(), 1);
                    else if (event.key.keysym.sym == SDLK_LEFTBRACKET || event.key.keysym.sym == SDLK_COMMA) m3d_session_select_variant_relative(session.get(), -1);
                    else if (event.key.keysym.sym == SDLK_RIGHTBRACKET || event.key.keysym.sym == SDLK_PERIOD) m3d_session_select_variant_relative(session.get(), 1);
                    else if (event.key.keysym.sym == SDLK_r) m3d_session_randomize(session.get());
                    else if (event.key.keysym.sym == SDLK_RETURN || event.key.keysym.sym == SDLK_SPACE) m3d_session_enter_lobby(session.get());
                    else if (event.key.keysym.sym == SDLK_BACKSPACE || event.key.keysym.sym == SDLK_ESCAPE) m3d_session_return_to_class_selector(session.get());
                } else if (mode == 2) {
                    if (event.key.keysym.sym == SDLK_LEFT || event.key.keysym.sym == SDLK_a) selectedWorld = (selectedWorld + WorldThemeCount - 1) % WorldThemeCount;
                    else if (event.key.keysym.sym == SDLK_RIGHT || event.key.keysym.sym == SDLK_d) selectedWorld = (selectedWorld + 1) % WorldThemeCount;
                    else if (event.key.keysym.sym == SDLK_RETURN || event.key.keysym.sym == SDLK_SPACE) m3d_session_enter_world(session.get());
                    else if (event.key.keysym.sym == SDLK_ESCAPE || event.key.keysym.sym == SDLK_BACKSPACE) m3d_session_return_to_customizer(session.get());
                } else if (event.key.keysym.sym == SDLK_ESCAPE || event.key.keysym.sym == SDLK_TAB) {
                    m3d_session_enter_lobby(session.get());
                    rightMouseDown = false;
                    SDL_SetRelativeMouseMode(SDL_FALSE);
                }
                break;
            case SDL_MOUSEBUTTONDOWN:
                if (event.button.button == SDL_BUTTON_RIGHT && mode == 3) {
                    rightMouseDown = true;
                    SDL_SetRelativeMouseMode(SDL_TRUE);
                } else if (event.button.button == SDL_BUTTON_LEFT && mode == 0) {
                    int ww = 1, wh = 1;
                    SDL_GetWindowSize(window, &ww, &wh);
                    const int count = m3d_session_character_count(session.get());
                    const int visible = std::min(count, 5);
                    const int slot = std::clamp(event.button.x * visible / std::max(1, ww), 0, visible - 1);
                    const int offset = slot - visible / 2;
                    const int index = ((m3d_session_selected_character(session.get()) + offset) % count + count) % count;
                    const bool same = index == m3d_session_selected_character(session.get());
                    m3d_session_select_character(session.get(), index);
                    if (event.button.clicks >= 2 || (same && event.button.y > wh * 0.86f)) m3d_session_open_customizer(session.get());
                } else if (event.button.button == SDL_BUTTON_LEFT && mode == 1) {
                    int ww = 1, wh = 1;
                    SDL_GetWindowSize(window, &ww, &wh);
                    if (event.button.x < ww * 0.22f && event.button.y > wh * 0.12f) {
                        const int group = static_cast<int>((event.button.y - wh * 0.15f) / std::max(1.0f, wh * 0.0625f));
                        m3d_session_select_group(session.get(), group);
                    } else if (event.button.x > ww * 0.72f && event.button.y > wh * 0.22f && event.button.y < wh * 0.48f) {
                        m3d_session_select_option_relative(session.get(), event.button.x < ww * 0.86f ? -1 : 1);
                    } else if (event.button.x > ww * 0.72f && event.button.y > wh * 0.48f && event.button.y < wh * 0.67f) {
                        m3d_session_select_variant_relative(session.get(), event.button.x < ww * 0.86f ? -1 : 1);
                    } else if (event.button.y > wh * 0.88f) {
                        m3d_session_enter_lobby(session.get());
                    }
                } else if (event.button.button == SDL_BUTTON_LEFT && mode == 2) {
                    int ww = 1, wh = 1;
                    SDL_GetWindowSize(window, &ww, &wh);
                    if (event.button.y > wh * 0.12f && event.button.y < wh * 0.28f && event.button.x > ww * 0.25f && event.button.x < ww * 0.62f) {
                        selectedWorld = (selectedWorld + (event.button.x < ww * 0.44f ? WorldThemeCount - 1 : 1)) % WorldThemeCount;
                    } else if (event.button.y > wh * 0.88f && event.button.x > ww * 0.38f && event.button.x < ww * 0.62f) {
                        m3d_session_enter_world(session.get());
                    }
                }
                break;
            case SDL_MOUSEBUTTONUP:
                if (event.button.button == SDL_BUTTON_RIGHT) {
                    rightMouseDown = false;
                    SDL_SetRelativeMouseMode(SDL_FALSE);
                }
                break;
            case SDL_MOUSEMOTION:
                if (rightMouseDown && mode == 3) cameraYaw -= static_cast<float>(event.motion.xrel) * 0.0045f;
                break;
            case SDL_MOUSEWHEEL:
                if (mode == 3) cameraDistance = clamp(cameraDistance - static_cast<float>(event.wheel.y) * 0.35f, 2.8f, 7.2f);
                break;
            default:
                break;
            }
        }

        const int currentMode = m3d_session_mode(session.get());
        if ((currentMode == 2 || currentMode == 3) && selectedWorld != loadedWorld) {
            try {
                if (!worlds[static_cast<std::size_t>(selectedWorld)]) {
                    worlds[static_cast<std::size_t>(selectedWorld)] = std::make_unique<VoxelWorld>();
                    worlds[static_cast<std::size_t>(selectedWorld)]->generate(static_cast<WorldTheme>(selectedWorld));
                }
                if (!renderer.uploadWorld(*worlds[static_cast<std::size_t>(selectedWorld)], error)) {
                    std::cerr << "No se pudo activar el mundo: " << error << "\n";
                    running = false;
                }
                loadedWorld = selectedWorld;
            } catch (const std::exception& exception) {
                std::cerr << "No se pudo generar el mundo: " << exception.what() << "\n";
                running = false;
            }
        }

        const std::uint64_t revision = m3d_session_customization_revision(session.get());
        if (revision != loadedRevision) {
            if (!loadout.sync(session.get(), assetCache, options.assetDirectory, options.sourceAssetDirectory, true, error)) {
                std::cerr << "No se pudo aplicar la personalización: " << error << "\n";
                running = false;
            }
            loadedRevision = revision;
        }

        const auto now = std::chrono::steady_clock::now();
        float delta = clamp(std::chrono::duration<float>(now - previousTime).count(), 0.0f, 0.05f);
        previousTime = now;
        if (options.demoMovement && !options.screenshotPath.empty()) delta = std::max(delta, 1.0f / 30.0f);
        const Uint8* keys = SDL_GetKeyboardState(nullptr);
        float forward = 0.0f, strafe = 0.0f;
        if (m3d_session_mode(session.get()) == 3 && !chatActive) {
            forward += (keys[SDL_SCANCODE_W] || keys[SDL_SCANCODE_UP]) ? 1.0f : 0.0f;
            forward -= (keys[SDL_SCANCODE_S] || keys[SDL_SCANCODE_DOWN]) ? 1.0f : 0.0f;
            strafe += (keys[SDL_SCANCODE_D] || keys[SDL_SCANCODE_RIGHT]) ? 1.0f : 0.0f;
            strafe -= (keys[SDL_SCANCODE_A] || keys[SDL_SCANCODE_LEFT]) ? 1.0f : 0.0f;
            if (options.demoMovement) forward = 1.0f;
        }
        const bool runInput = keys[SDL_SCANCODE_LSHIFT] || keys[SDL_SCANCODE_RSHIFT] || options.demoMovement;
        m3d_session_set_move(session.get(), forward, strafe, runInput ? 1 : 0, cameraYaw);
        m3d_session_update(session.get(), delta);

        const MultiplayerSnapshot network = multiplayer.snapshot();
        networkAccumulator += delta;
        if (m3d_session_mode(session.get()) == 3 && network.status == NetworkStatus::Connected && networkAccumulator >= 0.10f) {
            const PlayerState local = playerState(session.get());
            multiplayer.sendMove(local.position, local.yaw, local.speed, local.walkPhase,
                                 std::chrono::duration<float>(now - startTime).count(),
                                 m3d_session_selected_character(session.get()), selectedWorld);
            networkAccumulator = 0.0f;
        }

        int drawableWidth = 1, drawableHeight = 1;
        SDL_GL_GetDrawableSize(window, &drawableWidth, &drawableHeight);
        const float elapsed = std::chrono::duration<float>(now - startTime).count();
        const int mode = m3d_session_mode(session.get());
        if (mode == 0) renderer.renderClassSelector(loadout, session.get(), m3d_session_selector_time(session.get()), drawableWidth, drawableHeight);
        else if (mode == 1) renderer.renderCustomizer(loadout, session.get(), m3d_session_selector_time(session.get()), drawableWidth, drawableHeight);
        else if (mode == 2) renderer.renderLobby(loadout, session.get(), network, selectedWorld, chatActive, chatInput, elapsed, drawableWidth, drawableHeight);
        else renderer.renderWorld(loadout, playerState(session.get()), network, selectedWorld, chatActive, chatInput,
                                  cameraYaw, cameraDistance, elapsed, drawableWidth, drawableHeight);

        if (!options.screenshotPath.empty() && renderedFrames >= (options.demoMovement ? 30 : 6)) {
            if (!renderer.capturePng(options.screenshotPath, drawableWidth, drawableHeight, error)) std::cerr << error << "\n";
            else std::cout << "Captura guardada en " << options.screenshotPath << "\n";
            running = false;
        }
        SDL_GL_SwapWindow(window);
        ++renderedFrames;

        preloadAccumulator += delta;
        if (mode == 0 && preloadSession && preloadCursor < preloadOrder.size() && preloadAccumulator >= 0.08f) {
            m3d_session_select_character(preloadSession.get(), preloadOrder[preloadCursor++]);
            std::string preloadError;
            (void)preloadLoadout.sync(preloadSession.get(), assetCache, options.assetDirectory,
                                      options.sourceAssetDirectory, true, preloadError);
            preloadLoadout.clear();
            preloadAccumulator = 0.0f;
        }
        if (options.autoExitSeconds > 0.0f && elapsed >= options.autoExitSeconds) running = false;
    }

    SDL_StopTextInput();
    multiplayer.disconnect();
    SDL_SetRelativeMouseMode(SDL_FALSE); loadout.clear(); assetCache.clear(); renderer.shutdown();
    SDL_GL_DeleteContext(context); SDL_DestroyWindow(window); SDL_Quit(); return 0;
}

} // namespace m3d
