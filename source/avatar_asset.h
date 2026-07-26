#pragma once

#include "glad/glad.h"
#include "math_types.h"

#include <array>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace m3d {

struct AvatarVertex
{
    float position[3];
    float normal[3];
    float uv[2];
    float walkAPosition[3];
    float walkANormal[3];
    float walkBPosition[3];
    float walkBNormal[3];
    float runAPosition[3];
    float runANormal[3];
    float runBPosition[3];
    float runBNormal[3];
    float wavePosition[3];
    float waveNormal[3];
    float cheerPosition[3];
    float cheerNormal[3];
    float dancePosition[3];
    float danceNormal[3];
};
static_assert(sizeof(AvatarVertex) == sizeof(float) * 50);

struct AvatarPrimitive
{
    std::string label;
    std::vector<AvatarVertex> vertices;
    std::vector<std::uint32_t> indices;
    std::vector<std::uint8_t> imageBytes;
    std::array<float, 4> baseColor{1.0f, 1.0f, 1.0f, 1.0f};
    float alphaCutoff = 0.5f;
    std::uint32_t flags = 0;
    std::uint32_t meshSlot = 0;

    GLuint vao = 0;
    GLuint vertexBuffer = 0;
    GLuint indexBuffer = 0;
    GLuint texture = 0;
    GLsizei indexCount = 0;
};

class AvatarAsset
{
public:
    AvatarAsset() = default;
    ~AvatarAsset();
    AvatarAsset(const AvatarAsset&) = delete;
    AvatarAsset& operator=(const AvatarAsset&) = delete;

    bool load(const std::filesystem::path& path, std::string& error);
    bool upload(std::string& error);
    bool applyTextureVariant(const std::vector<std::filesystem::path>& paths, std::string& error);
    void clearVariant();
    void setTint(const Vec3& color, bool enabled = true) { tint_ = color; tintEnabled_ = enabled; }
    void clearTint() { tint_ = {1.0f, 1.0f, 1.0f}; tintEnabled_ = false; }
    void discardCpuData();
    void releaseGpu();
    static GLuint loadExternalTexture(const std::filesystem::path& path, std::string& error);

    const std::filesystem::path& path() const { return path_; }
    const std::vector<AvatarPrimitive>& primitives() const { return primitives_; }
    const Vec3& boundsMin() const { return boundsMin_; }
    const Vec3& boundsMax() const { return boundsMax_; }
    const Vec3& tint() const { return tint_; }
    bool tintEnabled() const { return tintEnabled_; }
    GLuint textureFor(const AvatarPrimitive& primitive) const;
    std::size_t vertexCount() const;
    std::size_t triangleCount() const { return triangleCount_; }

private:
    static GLuint loadTextureBytes(const unsigned char* bytes, int size, std::string& error);

    std::filesystem::path path_;
    std::vector<AvatarPrimitive> primitives_;
    std::vector<GLuint> variantTextures_;
    Vec3 boundsMin_{0.0f, 0.0f, 0.0f};
    Vec3 boundsMax_{0.0f, 0.0f, 0.0f};
    Vec3 tint_{1.0f, 1.0f, 1.0f};
    bool tintEnabled_ = false;
    std::size_t vertexCount_ = 0;
    std::size_t triangleCount_ = 0;
};

} // namespace m3d
