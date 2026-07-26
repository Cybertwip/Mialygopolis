#include "game_session.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace m3d {
namespace {

bool contains(const std::vector<std::string>& values, const std::string& value)
{
    return std::find(values.begin(), values.end(), value) != values.end();
}

} // namespace

bool GameSession::initialize(const std::filesystem::path& catalogPath, std::string& error)
{
    if (!catalog_.load(catalogPath, error)) return false;
    resetSelections();
    return !catalog_.characters().empty();
}

const CharacterDefinition& GameSession::character(std::size_t index) const
{
    if (index >= characterCount()) throw std::out_of_range("índice de personaje");
    return catalog_.characters()[index];
}

const CharacterDefinition& GameSession::selectedCharacterDefinition() const
{
    return character(static_cast<std::size_t>(selectedCharacter_));
}

const CharacterSelection& GameSession::selection() const
{
    return selections_[static_cast<std::size_t>(selectedCharacter_)];
}

void GameSession::resetSelections()
{
    selections_.clear();
    selections_.reserve(characterCount());
    for (const auto& definition : catalog_.characters()) {
        CharacterSelection selected;
        selected.options.reserve(definition.groups.size());
        selected.variants.reserve(definition.groups.size());
        for (const auto& group : definition.groups) {
            const int option = group.defaultOption >= 0 && group.defaultOption < static_cast<int>(group.options.size())
                ? group.defaultOption : (group.required && !group.options.empty() ? 0 : -1);
            selected.options.push_back(option);
            int variant = -1;
            if (option >= 0 && !group.options[static_cast<std::size_t>(option)].variants.empty()) {
                variant = std::max(0, group.options[static_cast<std::size_t>(option)].defaultVariant);
            }
            selected.variants.push_back(variant);
        }
        selections_.push_back(std::move(selected));
    }
}

int GameSession::selectedOption(int groupIndex) const
{
    if (groupIndex < 0 || groupIndex >= static_cast<int>(selection().options.size())) return -1;
    return selection().options[static_cast<std::size_t>(groupIndex)];
}

int GameSession::selectedVariant(int groupIndex) const
{
    if (groupIndex < 0 || groupIndex >= static_cast<int>(selection().variants.size())) return -1;
    return selection().variants[static_cast<std::size_t>(groupIndex)];
}

void GameSession::touchCustomization()
{
    ++customizationRevision_;
}

void GameSession::selectCharacter(int index)
{
    const int count = static_cast<int>(characterCount());
    if (count <= 0) return;
    const int next = ((index % count) + count) % count;
    if (next != selectedCharacter_) {
        selectedCharacter_ = next;
        activeGroup_ = 0;
        touchCustomization();
    }
}

void GameSession::selectRelative(int delta)
{
    selectCharacter(selectedCharacter_ + delta);
}

void GameSession::openCustomizer()
{
    mode_ = SessionMode::Customizer;
    activeGroup_ = std::clamp(activeGroup_, 0,
        std::max(0, static_cast<int>(selectedCharacterDefinition().groups.size()) - 1));
}

void GameSession::returnToClassSelector()
{
    mode_ = SessionMode::ClassSelector;
    player_.speed = 0.0f;
}

void GameSession::enterLobby()
{
    mode_ = SessionMode::Lobby;
    inputForward_ = 0.0f;
    inputStrafe_ = 0.0f;
    player_.speed = 0.0f;
}

void GameSession::enterWorld()
{
    mode_ = SessionMode::World;
    player_ = {};
    inputForward_ = 0.0f;
    inputStrafe_ = 0.0f;
}

void GameSession::returnToCustomizer()
{
    mode_ = SessionMode::Customizer;
    inputForward_ = 0.0f;
    inputStrafe_ = 0.0f;
    player_.speed = 0.0f;
}

void GameSession::selectGroup(int index)
{
    const int count = static_cast<int>(selectedCharacterDefinition().groups.size());
    if (count <= 0) return;
    activeGroup_ = ((index % count) + count) % count;
}

void GameSession::selectGroupRelative(int delta)
{
    selectGroup(activeGroup_ + delta);
}

void GameSession::selectOption(int groupIndex, int optionIndex)
{
    auto& selected = selections_[static_cast<std::size_t>(selectedCharacter_)];
    const auto& groups = selectedCharacterDefinition().groups;
    if (groupIndex < 0 || groupIndex >= static_cast<int>(groups.size())) return;
    const auto& group = groups[static_cast<std::size_t>(groupIndex)];
    if (group.required && optionIndex < 0) optionIndex = 0;
    if (optionIndex >= static_cast<int>(group.options.size())) optionIndex = static_cast<int>(group.options.size()) - 1;
    if (optionIndex < -1 || group.options.empty()) optionIndex = group.required && !group.options.empty() ? 0 : -1;
    if (selected.options[static_cast<std::size_t>(groupIndex)] == optionIndex) return;
    selected.options[static_cast<std::size_t>(groupIndex)] = optionIndex;
    selected.variants[static_cast<std::size_t>(groupIndex)] =
        optionIndex >= 0 && !group.options[static_cast<std::size_t>(optionIndex)].variants.empty() ? 0 : -1;
    applyRestrictions(groupIndex);
    touchCustomization();
}

void GameSession::selectOptionRelative(int delta)
{
    const auto& group = selectedCharacterDefinition().groups[static_cast<std::size_t>(activeGroup_)];
    const int count = static_cast<int>(group.options.size());
    if (count <= 0) return;
    if (group.required) {
        const int current = std::max(0, selectedOption(activeGroup_));
        selectOption(activeGroup_, ((current + delta) % count + count) % count);
    } else {
        const int sequenceCount = count + 1; // None plus actual options.
        const int current = selectedOption(activeGroup_) + 1;
        const int next = ((current + delta) % sequenceCount + sequenceCount) % sequenceCount;
        selectOption(activeGroup_, next - 1);
    }
}

void GameSession::selectVariant(int groupIndex, int variantIndex)
{
    auto& selected = selections_[static_cast<std::size_t>(selectedCharacter_)];
    const auto& groups = selectedCharacterDefinition().groups;
    if (groupIndex < 0 || groupIndex >= static_cast<int>(groups.size())) return;
    const int optionIndex = selectedOption(groupIndex);
    if (optionIndex < 0) return;
    const auto& variants = groups[static_cast<std::size_t>(groupIndex)].options[static_cast<std::size_t>(optionIndex)].variants;
    if (variants.empty()) return;
    variantIndex = ((variantIndex % static_cast<int>(variants.size())) + static_cast<int>(variants.size())) % static_cast<int>(variants.size());
    if (selected.variants[static_cast<std::size_t>(groupIndex)] != variantIndex) {
        selected.variants[static_cast<std::size_t>(groupIndex)] = variantIndex;
        touchCustomization();
    }
}

void GameSession::selectVariantRelative(int delta)
{
    selectVariant(activeGroup_, selectedVariant(activeGroup_) + delta);
}

void GameSession::randomize()
{
    const auto& groups = selectedCharacterDefinition().groups;
    for (int groupIndex = 0; groupIndex < static_cast<int>(groups.size()); ++groupIndex) {
        const auto& group = groups[static_cast<std::size_t>(groupIndex)];
        if (group.options.empty()) continue;
        int option = 0;
        if (!group.required) {
            std::uniform_int_distribution<int> optionDistribution(-1, static_cast<int>(group.options.size()) - 1);
            option = optionDistribution(random_);
        } else {
            std::uniform_int_distribution<int> optionDistribution(0, static_cast<int>(group.options.size()) - 1);
            option = optionDistribution(random_);
        }
        selections_[static_cast<std::size_t>(selectedCharacter_)].options[static_cast<std::size_t>(groupIndex)] = option;
        int variant = -1;
        if (option >= 0) {
            const auto& variants = group.options[static_cast<std::size_t>(option)].variants;
            if (!variants.empty()) {
                std::uniform_int_distribution<int> variantDistribution(0, static_cast<int>(variants.size()) - 1);
                variant = variantDistribution(random_);
            }
        }
        selections_[static_cast<std::size_t>(selectedCharacter_)].variants[static_cast<std::size_t>(groupIndex)] = variant;
        applyRestrictions(groupIndex);
    }
    touchCustomization();
}

bool GameSession::optionHasType(int groupIndex, const std::string& type) const
{
    const int option = selectedOption(groupIndex);
    if (option < 0) return false;
    const auto& selectedOptionData = selectedCharacterDefinition().groups[static_cast<std::size_t>(groupIndex)].options[static_cast<std::size_t>(option)];
    return contains(selectedOptionData.types, type);
}

void GameSession::removeOptionalGroup(int groupIndex)
{
    const auto& group = selectedCharacterDefinition().groups[static_cast<std::size_t>(groupIndex)];
    if (group.required) return;
    auto& selected = selections_[static_cast<std::size_t>(selectedCharacter_)];
    selected.options[static_cast<std::size_t>(groupIndex)] = -1;
    selected.variants[static_cast<std::size_t>(groupIndex)] = -1;
}

void GameSession::applyRestrictions(int changedGroup)
{
    const auto& definition = selectedCharacterDefinition();
    const int changedOption = selectedOption(changedGroup);
    if (changedOption < 0) return;
    const auto& changedGroupData = definition.groups[static_cast<std::size_t>(changedGroup)];
    const auto& changedOptionData = changedGroupData.options[static_cast<std::size_t>(changedOption)];

    if (const auto iterator = definition.traitRestrictions.find(changedGroupData.id);
        iterator != definition.traitRestrictions.end()) {
        for (int groupIndex = 0; groupIndex < static_cast<int>(definition.groups.size()); ++groupIndex) {
            if (groupIndex == changedGroup) continue;
            const auto& candidate = definition.groups[static_cast<std::size_t>(groupIndex)];
            if (contains(iterator->second.restrictedTraits, candidate.id)) removeOptionalGroup(groupIndex);
            for (const auto& type : iterator->second.restrictedTypes) {
                if (optionHasType(groupIndex, type)) removeOptionalGroup(groupIndex);
            }
        }
    }

    for (const auto& selectedType : changedOptionData.types) {
        const auto restriction = definition.typeRestrictions.find(selectedType);
        if (restriction == definition.typeRestrictions.end()) continue;
        for (int groupIndex = 0; groupIndex < static_cast<int>(definition.groups.size()); ++groupIndex) {
            if (groupIndex == changedGroup) continue;
            for (const auto& restrictedType : restriction->second) {
                if (optionHasType(groupIndex, restrictedType)) removeOptionalGroup(groupIndex);
            }
        }
    }
}

void GameSession::setMoveInput(float forward, float strafe, bool running, float viewYaw)
{
    inputForward_ = clamp(forward, -1.0f, 1.0f);
    inputStrafe_ = clamp(strafe, -1.0f, 1.0f);
    running_ = running;
    viewYaw_ = viewYaw;
}

void GameSession::update(float deltaSeconds)
{
    deltaSeconds = clamp(deltaSeconds, 0.0f, 0.05f);
    selectorTime_ += deltaSeconds;
    if (mode_ != SessionMode::World) return;

    const float inputLength = std::sqrt(inputForward_ * inputForward_ + inputStrafe_ * inputStrafe_);
    if (inputLength < 0.01f) {
        player_.speed += (0.0f - player_.speed) * std::min(1.0f, deltaSeconds * 10.0f);
        return;
    }

    const float forward = inputForward_ / std::max(1.0f, inputLength);
    const float strafe = inputStrafe_ / std::max(1.0f, inputLength);
    // Camera look direction is the movement frame. In this view convention the
    // screen-right vector is -X at yaw zero, hence the negative cosine.
    const Vec3 cameraForward{std::sin(viewYaw_), 0.0f, std::cos(viewYaw_)};
    const Vec3 cameraRight{-std::cos(viewYaw_), 0.0f, std::sin(viewYaw_)};
    Vec3 direction = linalg::normalize(cameraForward * forward + cameraRight * strafe);

    const float targetSpeed = running_ ? 4.2f : 2.45f;
    player_.speed += (targetSpeed - player_.speed) * std::min(1.0f, deltaSeconds * 8.0f);
    player_.position += direction * (player_.speed * deltaSeconds);

    const float radius = std::sqrt(player_.position.x * player_.position.x + player_.position.z * player_.position.z);
    constexpr float walkableRadius = 8.6f;
    if (radius > walkableRadius) {
        const float scale = walkableRadius / radius;
        player_.position.x *= scale;
        player_.position.z *= scale;
    }

    const float desiredYaw = std::atan2(direction.x, direction.z);
    float yawDelta = desiredYaw - player_.yaw;
    while (yawDelta > Pi) yawDelta -= 2.0f * Pi;
    while (yawDelta < -Pi) yawDelta += 2.0f * Pi;
    player_.yaw += yawDelta * std::min(1.0f, deltaSeconds * 12.0f);
    player_.walkPhase += player_.speed * deltaSeconds * (running_ ? 5.8f : 4.5f);
}

} // namespace m3d
