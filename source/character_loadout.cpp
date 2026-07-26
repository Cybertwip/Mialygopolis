#include "character_loadout.h"

#include <algorithm>
#include <sstream>

namespace m3d {

std::shared_ptr<AvatarAsset> CharacterAssetCache::asset(const std::filesystem::path& path,
                                                        bool uploadGpu, std::string& error)
{
    const std::string key = std::filesystem::weakly_canonical(path).string();
    if (const auto found = assets_.find(key); found != assets_.end()) return found->second;

    auto loaded = std::make_shared<AvatarAsset>();
    if (!loaded->load(path, error)) return {};
    if (uploadGpu) {
        if (!loaded->upload(error)) return {};
        loaded->discardCpuData();
    }
    assets_.emplace(key, loaded);
    return loaded;
}

GLuint CharacterAssetCache::texture(const std::filesystem::path& path, std::string& error)
{
    const std::string key = std::filesystem::weakly_canonical(path).string();
    if (const auto found = textures_.find(key); found != textures_.end()) return found->second;
    const GLuint loaded = AvatarAsset::loadExternalTexture(path, error);
    if (loaded) textures_.emplace(key, loaded);
    return loaded;
}

void CharacterAssetCache::clear()
{
    for (auto& [_, asset] : assets_) asset->releaseGpu();
    assets_.clear();
    for (const auto& [_, texture] : textures_) glDeleteTextures(1, &texture);
    textures_.clear();
}

bool CharacterLoadout::sync(const M3DSession* session,
                            CharacterAssetCache& cache,
                            const std::filesystem::path& generatedAssetDirectory,
                            const std::filesystem::path& sourceAssetDirectory,
                            bool uploadGpu,
                            std::string& error)
{
    if (!session) {
        error = "No se puede construir un personaje sin una sesión";
        return false;
    }

    const int selectedCharacter = m3d_session_selected_character(session);
    const M3DCharacterInfo character = m3d_session_character(session, selectedCharacter);
    if (!character.id) {
        error = "El personaje seleccionado no aparece en el catálogo";
        return false;
    }
    accent_ = {character.accent_r, character.accent_g, character.accent_b};
    name_ = character.name ? character.name : "PERSONAJE";
    tagline_ = character.tagline ? character.tagline : "";
    const Vec3 boundsMin{character.bounds_min_x, character.bounds_min_y, character.bounds_min_z};
    const Vec3 boundsMax{character.bounds_max_x, character.bounds_max_y, character.bounds_max_z};
    localOffset_ = {-(boundsMin.x + boundsMax.x) * 0.5f, -boundsMin.y,
                    -(boundsMin.z + boundsMax.z) * 0.5f};
    height_ = std::max(0.1f, boundsMax.y - boundsMin.y);

    const int groupCount = m3d_session_group_count(session);
    if (selectedCharacter != characterIndex_ || static_cast<int>(parts_.size()) != groupCount) {
        parts_.clear();
        parts_.resize(static_cast<std::size_t>(groupCount));
        for (int groupIndex = 0; groupIndex < groupCount; ++groupIndex) {
            parts_[static_cast<std::size_t>(groupIndex)].groupIndex = groupIndex;
        }
        characterIndex_ = selectedCharacter;
    }

    int visiblePartCount = 0;
    for (int groupIndex = 0; groupIndex < groupCount; ++groupIndex) {
        CharacterPart& part = parts_[static_cast<std::size_t>(groupIndex)];
        const int optionIndex = m3d_session_selected_option(session, groupIndex);
        const int variantIndex = m3d_session_selected_variant(session, groupIndex);
        if (optionIndex < 0) {
            part = CharacterPart{groupIndex};
            continue;
        }
        const M3DOptionInfo option = m3d_session_option(session, groupIndex, optionIndex);
        if (!option.asset_file) continue;
        std::ostringstream key;
        key << option.asset_file << '#' << variantIndex;
        if (part.stateKey == key.str() && part.asset) {
            ++visiblePartCount;
            continue;
        }

        CharacterPart next;
        next.groupIndex = groupIndex;
        next.stateKey = key.str();
        next.asset = cache.asset(generatedAssetDirectory / option.asset_file, uploadGpu, error);
        if (!next.asset) return false;

        if (variantIndex >= 0) {
            const M3DVariantInfo variant = m3d_session_variant(session, groupIndex, optionIndex, variantIndex);
            if (uploadGpu) {
                next.variantTextures.reserve(static_cast<std::size_t>(variant.texture_count));
                for (int textureIndex = 0; textureIndex < variant.texture_count; ++textureIndex) {
                    const char* relative = m3d_session_variant_texture(
                        session, groupIndex, optionIndex, variantIndex, textureIndex);
                    if (!relative) continue;
                    const GLuint texture = cache.texture(sourceAssetDirectory / relative, error);
                    if (!texture) return false;
                    next.variantTextures.push_back(texture);
                }
            }
            if (variant.color_count > 0) {
                float r = 1.0f, g = 1.0f, b = 1.0f;
                m3d_session_variant_color(session, groupIndex, optionIndex, variantIndex, 0, &r, &g, &b);
                next.tint = {r, g, b};
                next.tintEnabled = true;
            }
        }
        part = std::move(next);
        ++visiblePartCount;
    }

    if (visiblePartCount == 0) {
        error = "La personalización actual no contiene rasgos visibles";
        return false;
    }
    return true;
}

void CharacterLoadout::clear()
{
    parts_.clear();
    characterIndex_ = -1;
}

std::size_t CharacterLoadout::vertexCount() const
{
    std::size_t count = 0;
    for (const auto& part : parts_) if (part.asset) count += part.asset->vertexCount();
    return count;
}

std::size_t CharacterLoadout::triangleCount() const
{
    std::size_t count = 0;
    for (const auto& part : parts_) if (part.asset) count += part.asset->triangleCount();
    return count;
}

} // namespace m3d
