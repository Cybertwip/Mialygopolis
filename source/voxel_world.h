#pragma once

#include "extraction.h"
#include "math_types.h"
#include "storage.h"

#include <cstddef>
#include <span>
#include <string_view>
#include <vector>

namespace m3d {

enum class WorldTheme : int
{
    RomanCity = 0,
    GreekHeaven = 1,
};

constexpr int WorldThemeCount = 2;
std::string_view worldName(WorldTheme theme);
std::string_view worldDescription(WorldTheme theme);


struct DynamicCollider
{
    Vec3 position{0.0f, 0.13f, 0.0f};
    float radius = 0.32f;
    float height = 1.7f;
};

class VoxelWorld
{
public:
    static constexpr float VoxelScale = 0.25f;

    void generate(WorldTheme theme = WorldTheme::RomanCity);

    const std::vector<Cubiquity::Glyph>& glyphs() const { return glyphs_; }
    std::size_t nodeCount() const { return volume_.countNodes(); }
    WorldTheme theme() const { return theme_; }
    Vec3 skyColor() const;
    Vec3 resolvePlayerMotion(const Vec3& from, const Vec3& proposed,
                             float radius, float height,
                             std::span<const DynamicCollider> dynamicColliders = {}) const;
    float groundHeight(const Vec3& position, float maxStep = 0.45f) const;

private:
    void generateRomanCity();
    void generateGreekHeaven();
    void finishGeneration();
    bool collidesCapsule(const Vec3& position, float radius, float height,
                         std::span<const DynamicCollider> dynamicColliders) const;
    bool isCollidable(Cubiquity::MaterialId material) const;

    void setBox(int x0, int y0, int z0, int x1, int y1, int z1, Cubiquity::MaterialId material);
    void setHollowBox(int x0, int y0, int z0, int x1, int y1, int z1, Cubiquity::MaterialId material);
    void setRomanColumn(int x, int y, int z, int height, Cubiquity::MaterialId material);
    void setCypress(int x, int z, int height);
    void setTree(int x, int z, int height, int crownRadius, Cubiquity::MaterialId foliage);
    void setSteppedRoof(int x0, int z0, int x1, int z1, int baseY, int layers, Cubiquity::MaterialId material);

    Cubiquity::Volume volume_;
    std::vector<Cubiquity::Glyph> glyphs_;
    WorldTheme theme_ = WorldTheme::RomanCity;
};

} // namespace m3d
