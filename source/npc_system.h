#pragma once

#include "math_types.h"
#include "voxel_world.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace m3d {

struct NpcDefinition
{
    std::string id;
    std::string name;
    std::string title;
    std::string aiPersona;
    Vec3 position{0.0f, 0.13f, 0.0f};
    float yaw = 0.0f;
    int characterIndex = 0;
    float colliderRadius = 0.34f;
    std::vector<std::string> placeholders;
};

struct NpcDialogueView
{
    bool active = false;
    std::string npcId;
    std::string name;
    std::string title;
    std::string text;
};

struct NpcInferenceRequest
{
    std::uint64_t requestId = 0;
    std::string npcId;
    std::string npcName;
    std::string persona;
    std::string playerText;
    std::string world;
};

using NpcInferenceCallback = bool (*)(const char* requestJson,
                                      char* responseUtf8,
                                      std::size_t responseCapacity,
                                      void* userData);

class NpcSystem
{
public:
    void configure(WorldTheme theme);
    const std::vector<NpcDefinition>& npcs() const { return npcs_; }
    std::vector<DynamicCollider> colliders() const;

    const NpcDefinition* nearest(const Vec3& playerPosition, float maxDistance = 1.65f) const;
    bool interact(const Vec3& playerPosition, std::string_view playerText = {});
    void closeDialogue() { dialogue_ = {}; }
    const NpcDialogueView& dialogue() const { return dialogue_; }

    void setInferenceCallback(NpcInferenceCallback callback, void* userData)
    {
        inferenceCallback_ = callback;
        inferenceUserData_ = userData;
    }

private:
    std::string placeholderFor(const NpcDefinition& npc);
    std::string requestJson(const NpcInferenceRequest& request) const;

    WorldTheme theme_ = WorldTheme::RomanCity;
    std::vector<NpcDefinition> npcs_;
    NpcDialogueView dialogue_;
    std::uint64_t nextRequestId_ = 1;
    std::size_t placeholderCursor_ = 0;
    NpcInferenceCallback inferenceCallback_ = nullptr;
    void* inferenceUserData_ = nullptr;
};

} // namespace m3d
