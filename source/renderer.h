#pragma once

#include "bindings.h"
#include "character_loadout.h"
#include "glad/glad.h"
#include "game_session.h"
#include "math_types.h"
#include "multiplayer_client.h"
#include "ui_renderer.h"
#include "voxel_world.h"

#include <filesystem>
#include <string>

namespace m3d {

class Renderer
{
public:
    bool initialize(std::string& error);
    bool uploadWorld(const VoxelWorld& world, std::string& error);
    void shutdown();

    void renderClassSelector(const CharacterLoadout& loadout, const M3DSession* session,
                             float timeSeconds, int width, int height);
    void renderCustomizer(const CharacterLoadout& loadout, const M3DSession* session,
                          float timeSeconds, int width, int height);
    void renderLobby(const CharacterLoadout& loadout, const M3DSession* session,
                     const MultiplayerSnapshot& multiplayer, int selectedWorld,
                     bool chatActive, const std::string& chatInput,
                     float timeSeconds, int width, int height);
    void renderWorld(const CharacterLoadout& loadout, const PlayerState& player,
                     const MultiplayerSnapshot& multiplayer, int selectedWorld,
                     bool chatActive, const std::string& chatInput, float cameraYaw, float cameraDistance,
                     float timeSeconds, int width, int height);

    bool capturePng(const std::filesystem::path& path, int width, int height, std::string& error) const;

private:
    bool createPrograms(std::string& error);
    bool createWorldGeometry(std::string& error);
    void drawWorld(const Mat4& view, const Mat4& projection, const Vec3& cameraPosition);
    void drawLoadout(const CharacterLoadout& loadout, const Mat4& model, const Mat4& view,
                     const Mat4& projection, const Vec3& cameraPosition,
                     float accentMix, float phase, float motion);
    void drawClassSelectorUi(const M3DSession* session, int width, int height);
    void drawCustomizerUi(const M3DSession* session, int width, int height);
    void drawLobbyUi(const M3DSession* session, const MultiplayerSnapshot& multiplayer, int selectedWorld, bool chatActive, const std::string& chatInput, int width, int height);
    void drawWorldUi(const CharacterLoadout& loadout, const PlayerState& player, const MultiplayerSnapshot& multiplayer, int selectedWorld, bool chatActive, const std::string& chatInput, int width, int height);
    void drawChatUi(const MultiplayerSnapshot& multiplayer, bool chatActive, const std::string& chatInput, int width, int height, float scale);

    GLuint worldProgram_ = 0;
    GLuint avatarProgram_ = 0;
    GLuint worldVao_ = 0;
    GLuint worldVertexBuffer_ = 0;
    GLuint worldInstanceBuffer_ = 0;
    GLsizei worldGlyphCount_ = 0;
    Vec3 worldSkyColor_{0.20f, 0.43f, 0.72f};
    UiRenderer ui_;
};

} // namespace m3d
