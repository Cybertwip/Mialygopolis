#pragma once

#include "extraction.h"
#include "math_types.h"
#include "storage.h"

#include <cstddef>
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

class VoxelWorld
{
public:
    static constexpr float VoxelScale = 0.25f;

    void generate(WorldTheme theme = WorldTheme::RomanCity);

    const std::vector<Cubiquity::Glyph>& glyphs() const { return glyphs_; }
    std::size_t nodeCount() const { return volume_.countNodes(); }
    WorldTheme theme() const { return theme_; }
    Vec3 skyColor() const;

private:
    void generateRomanCity();
    void generateGreekHeaven();
    void finishGeneration();

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
