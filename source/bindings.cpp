#include "bindings.h"

#include "game_session.h"

#include <new>
#include <string>

struct M3DSession
{
    m3d::GameSession value;
};

namespace {
thread_local std::string lastError;

const m3d::CharacterGroup* groupAt(const M3DSession* session, int groupIndex)
{
    if (!session || groupIndex < 0) return nullptr;
    const auto& groups = session->value.selectedCharacterDefinition().groups;
    return groupIndex < static_cast<int>(groups.size()) ? &groups[static_cast<std::size_t>(groupIndex)] : nullptr;
}

const m3d::CharacterOption* optionAt(const M3DSession* session, int groupIndex, int optionIndex)
{
    const auto* group = groupAt(session, groupIndex);
    if (!group || optionIndex < 0 || optionIndex >= static_cast<int>(group->options.size())) return nullptr;
    return &group->options[static_cast<std::size_t>(optionIndex)];
}

const m3d::CharacterVariant* variantAt(const M3DSession* session, int groupIndex, int optionIndex, int variantIndex)
{
    const auto* option = optionAt(session, groupIndex, optionIndex);
    if (!option || variantIndex < 0 || variantIndex >= static_cast<int>(option->variants.size())) return nullptr;
    return &option->variants[static_cast<std::size_t>(variantIndex)];
}
}

extern "C" {

M3DSession* m3d_session_create(const char* catalogPath)
{
    lastError.clear();
    if (!catalogPath) {
        lastError = "No se proporcionó la ruta del catálogo de personajes";
        return nullptr;
    }
    M3DSession* session = new (std::nothrow) M3DSession{};
    if (!session) {
        lastError = "No se pudo reservar una sesión de juego";
        return nullptr;
    }
    if (!session->value.initialize(catalogPath, lastError)) {
        delete session;
        return nullptr;
    }
    return session;
}

void m3d_session_destroy(M3DSession* session) { delete session; }
const char* m3d_last_error(void) { return lastError.c_str(); }

int m3d_session_character_count(const M3DSession* session)
{
    return session ? static_cast<int>(session->value.characterCount()) : 0;
}

M3DCharacterInfo m3d_session_character(const M3DSession* session, int index)
{
    if (!session || index < 0 || index >= m3d_session_character_count(session)) return {};
    const auto& info = session->value.character(static_cast<std::size_t>(index));
    return {info.id.c_str(), info.name.c_str(), info.tagline.c_str(),
            info.accent.x, info.accent.y, info.accent.z,
            info.boundsMin.x, info.boundsMin.y, info.boundsMin.z,
            info.boundsMax.x, info.boundsMax.y, info.boundsMax.z};
}

int m3d_session_mode(const M3DSession* session) { return session ? static_cast<int>(session->value.mode()) : 0; }
int m3d_session_selected_character(const M3DSession* session) { return session ? session->value.selectedCharacter() : 0; }
float m3d_session_selector_time(const M3DSession* session) { return session ? session->value.selectorTime() : 0.0f; }
uint64_t m3d_session_customization_revision(const M3DSession* session) { return session ? session->value.customizationRevision() : 0; }

M3DPlayerState m3d_session_player(const M3DSession* session)
{
    if (!session) return {};
    const auto& player = session->value.player();
    return {player.position.x, player.position.y, player.position.z,
            player.yaw, player.speed, player.walkPhase};
}

void m3d_session_select_character(M3DSession* session, int index) { if (session) session->value.selectCharacter(index); }
void m3d_session_select_relative(M3DSession* session, int delta) { if (session) session->value.selectRelative(delta); }
void m3d_session_open_customizer(M3DSession* session) { if (session) session->value.openCustomizer(); }
void m3d_session_return_to_class_selector(M3DSession* session) { if (session) session->value.returnToClassSelector(); }
void m3d_session_enter_lobby(M3DSession* session) { if (session) session->value.enterLobby(); }
void m3d_session_enter_world(M3DSession* session) { if (session) session->value.enterWorld(); }
void m3d_session_return_to_customizer(M3DSession* session) { if (session) session->value.returnToCustomizer(); }

int m3d_session_group_count(const M3DSession* session)
{
    return session ? static_cast<int>(session->value.selectedCharacterDefinition().groups.size()) : 0;
}

M3DGroupInfo m3d_session_group(const M3DSession* session, int groupIndex)
{
    const auto* group = groupAt(session, groupIndex);
    return group ? M3DGroupInfo{group->id.c_str(), group->name.c_str(), group->required ? 1 : 0,
                               static_cast<int>(group->options.size())} : M3DGroupInfo{};
}

int m3d_session_active_group(const M3DSession* session) { return session ? session->value.activeGroup() : 0; }
void m3d_session_select_group(M3DSession* session, int groupIndex) { if (session) session->value.selectGroup(groupIndex); }
void m3d_session_select_group_relative(M3DSession* session, int delta) { if (session) session->value.selectGroupRelative(delta); }

int m3d_session_selected_option(const M3DSession* session, int groupIndex)
{
    return session ? session->value.selectedOption(groupIndex) : -1;
}

M3DOptionInfo m3d_session_option(const M3DSession* session, int groupIndex, int optionIndex)
{
    const auto* option = optionAt(session, groupIndex, optionIndex);
    return option ? M3DOptionInfo{option->id.c_str(), option->name.c_str(), option->assetFile.c_str(),
                                  option->thumbnail.c_str(), static_cast<int>(option->variantKind),
                                  static_cast<int>(option->variants.size())} : M3DOptionInfo{};
}

void m3d_session_select_option(M3DSession* session, int groupIndex, int optionIndex)
{
    if (session) session->value.selectOption(groupIndex, optionIndex);
}
void m3d_session_select_option_relative(M3DSession* session, int delta)
{
    if (session) session->value.selectOptionRelative(delta);
}

int m3d_session_selected_variant(const M3DSession* session, int groupIndex)
{
    return session ? session->value.selectedVariant(groupIndex) : -1;
}

M3DVariantInfo m3d_session_variant(const M3DSession* session, int groupIndex, int optionIndex, int variantIndex)
{
    const auto* variant = variantAt(session, groupIndex, optionIndex, variantIndex);
    return variant ? M3DVariantInfo{variant->id.c_str(), variant->name.c_str(),
                                    static_cast<int>(variant->texturePaths.size()),
                                    static_cast<int>(variant->colors.size())} : M3DVariantInfo{};
}

const char* m3d_session_variant_texture(const M3DSession* session, int groupIndex, int optionIndex,
                                        int variantIndex, int textureIndex)
{
    const auto* variant = variantAt(session, groupIndex, optionIndex, variantIndex);
    if (!variant || textureIndex < 0 || textureIndex >= static_cast<int>(variant->texturePaths.size())) return nullptr;
    return variant->texturePaths[static_cast<std::size_t>(textureIndex)].c_str();
}

void m3d_session_variant_color(const M3DSession* session, int groupIndex, int optionIndex,
                               int variantIndex, int colorIndex, float* r, float* g, float* b)
{
    const auto* variant = variantAt(session, groupIndex, optionIndex, variantIndex);
    if (!variant || colorIndex < 0 || colorIndex >= static_cast<int>(variant->colors.size())) return;
    const auto color = variant->colors[static_cast<std::size_t>(colorIndex)];
    if (r) *r = color.x;
    if (g) *g = color.y;
    if (b) *b = color.z;
}

void m3d_session_select_variant(M3DSession* session, int groupIndex, int variantIndex)
{
    if (session) session->value.selectVariant(groupIndex, variantIndex);
}
void m3d_session_select_variant_relative(M3DSession* session, int delta)
{
    if (session) session->value.selectVariantRelative(delta);
}
void m3d_session_randomize(M3DSession* session) { if (session) session->value.randomize(); }

void m3d_session_set_move(M3DSession* session, float forward, float strafe, int running, float viewYaw)
{
    if (session) session->value.setMoveInput(forward, strafe, running != 0, viewYaw);
}
void m3d_session_set_player_position(M3DSession* session, float x, float y, float z)
{
    if (session) session->value.setPlayerPosition({x, y, z});
}
void m3d_session_update(M3DSession* session, float deltaSeconds)
{
    if (session) session->value.update(deltaSeconds);
}

} // extern "C"
