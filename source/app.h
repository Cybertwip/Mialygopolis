#pragma once

#include <filesystem>
#include <string>

namespace m3d {

struct AppOptions
{
    std::filesystem::path assetDirectory;
    std::filesystem::path sourceAssetDirectory;
    std::filesystem::path screenshotPath;
    std::string serverAddress = "127.0.0.1:7777";
    std::string playerName;
    std::string roleToken;
    bool smokeTest = false;
    bool startInCustomizer = false;
    bool startInLobby = false;
    bool startInWorld = false;
    bool demoMovement = false;
    bool offline = false;
    int selectedCharacter = 0;
    int selectedWorld = 0;
    int width = 1280;
    int height = 720;
    float autoExitSeconds = 0.0f;
};

class Application
{
public:
    int run(const AppOptions& options);

private:
    int runSmokeTest(const AppOptions& options);
    int runInteractive(const AppOptions& options);
};

} // namespace m3d
