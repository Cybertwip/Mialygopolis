#pragma once

#include "avatar_asset.h"
#include "bindings.h"
#include "math_types.h"

#include <filesystem>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace m3d {

class CharacterAssetCache
{
public:
    std::shared_ptr<AvatarAsset> asset(const std::filesystem::path& path, bool uploadGpu, std::string& error);
    GLuint texture(const std::filesystem::path& path, std::string& error);
    void clear();
    std::size_t assetCount() const { return assets_.size(); }
    std::size_t textureCount() const { return textures_.size(); }

private:
    std::unordered_map<std::string, std::shared_ptr<AvatarAsset>> assets_;
    std::unordered_map<std::string, GLuint> textures_;
};

struct CharacterPart
{
    int groupIndex = -1;
    std::string stateKey;
    std::shared_ptr<AvatarAsset> asset;
    std::vector<GLuint> variantTextures;
    Vec3 tint{1.0f, 1.0f, 1.0f};
    bool tintEnabled = false;

    GLuint textureFor(const AvatarPrimitive& primitive) const
    {
        if (variantTextures.empty()) return primitive.texture;
        return variantTextures[std::min<std::size_t>(primitive.meshSlot, variantTextures.size() - 1)];
    }
};

class CharacterLoadout
{
public:
    bool sync(const M3DSession* session,
              CharacterAssetCache& cache,
              const std::filesystem::path& generatedAssetDirectory,
              const std::filesystem::path& sourceAssetDirectory,
              bool uploadGpu,
              std::string& error);
    void clear();

    const std::vector<CharacterPart>& parts() const { return parts_; }
    const Vec3& localOffset() const { return localOffset_; }
    float height() const { return height_; }
    const Vec3& accent() const { return accent_; }
    const std::string& name() const { return name_; }
    const std::string& tagline() const { return tagline_; }
    std::size_t vertexCount() const;
    std::size_t triangleCount() const;

private:
    std::vector<CharacterPart> parts_;
    int characterIndex_ = -1;
    Vec3 localOffset_{0.0f, 0.0f, 0.0f};
    Vec3 accent_{1.0f, 1.0f, 1.0f};
    float height_ = 1.7f;
    std::string name_;
    std::string tagline_;
};

} // namespace m3d
