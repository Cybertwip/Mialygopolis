#pragma once

#include "character_catalog.h"
#include "math_types.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <random>
#include <string>
#include <vector>

namespace m3d {

enum class SessionMode : int
{
    ClassSelector = 0,
    Customizer = 1,
    Lobby = 2,
    World = 3,
};

struct PlayerState
{
    Vec3 position{0.0f, 0.13f, 0.0f};
    float yaw = 0.0f;
    float speed = 0.0f;
    float walkPhase = 0.0f;
};

struct CharacterSelection
{
    std::vector<int> options;
    std::vector<int> variants;
};

class GameSession
{
public:
    bool initialize(const std::filesystem::path& catalogPath, std::string& error);

    const CharacterCatalog& catalog() const { return catalog_; }
    std::size_t characterCount() const { return catalog_.characters().size(); }
    const CharacterDefinition& character(std::size_t index) const;
    const CharacterDefinition& selectedCharacterDefinition() const;
    const CharacterSelection& selection() const;

    SessionMode mode() const { return mode_; }
    int selectedCharacter() const { return selectedCharacter_; }
    int activeGroup() const { return activeGroup_; }
    const PlayerState& player() const { return player_; }
    float selectorTime() const { return selectorTime_; }
    std::uint64_t customizationRevision() const { return customizationRevision_; }

    int selectedOption(int groupIndex) const;
    int selectedVariant(int groupIndex) const;

    void selectCharacter(int index);
    void selectRelative(int delta);
    void openCustomizer();
    void returnToClassSelector();
    void enterLobby();
    void enterWorld();
    void returnToCustomizer();

    void selectGroup(int index);
    void selectGroupRelative(int delta);
    void selectOption(int groupIndex, int optionIndex);
    void selectOptionRelative(int delta);
    void selectVariant(int groupIndex, int variantIndex);
    void selectVariantRelative(int delta);
    void randomize();

    void setMoveInput(float forward, float strafe, bool running, float viewYaw);
    void setPlayerPosition(const Vec3& position) { player_.position = position; }
    void update(float deltaSeconds);

private:
    void resetSelections();
    void touchCustomization();
    void applyRestrictions(int changedGroup);
    bool optionHasType(int groupIndex, const std::string& type) const;
    void removeOptionalGroup(int groupIndex);

    CharacterCatalog catalog_;
    std::vector<CharacterSelection> selections_;
    SessionMode mode_ = SessionMode::ClassSelector;
    int selectedCharacter_ = 0;
    int activeGroup_ = 0;
    PlayerState player_{};
    float inputForward_ = 0.0f;
    float inputStrafe_ = 0.0f;
    float viewYaw_ = 0.0f;
    bool running_ = false;
    float selectorTime_ = 0.0f;
    std::uint64_t customizationRevision_ = 1;
    std::mt19937 random_{0x4d3344u};
};

} // namespace m3d
