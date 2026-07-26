#pragma once

#include "math_types.h"

#include <cstdint>
#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

namespace m3d {

enum class VariantKind : std::uint8_t
{
    None = 0,
    Texture = 1,
    Color = 2,
};

struct CharacterVariant
{
    std::string id;
    std::string name;
    std::vector<std::string> texturePaths;
    std::vector<Vec3> colors;
};

struct CharacterOption
{
    std::string id;
    std::string name;
    std::string assetFile;
    std::string thumbnail;
    std::vector<std::string> types;
    VariantKind variantKind = VariantKind::None;
    int defaultVariant = -1;
    std::vector<CharacterVariant> variants;
};

struct CharacterGroup
{
    std::string id;
    std::string name;
    bool required = false;
    int defaultOption = -1;
    std::vector<CharacterOption> options;
};

struct TraitRestriction
{
    std::vector<std::string> restrictedTraits;
    std::vector<std::string> restrictedTypes;
};

struct CharacterDefinition
{
    std::string id;
    std::string name;
    std::string tagline;
    Vec3 accent{1.0f, 1.0f, 1.0f};
    Vec3 boundsMin{-0.5f, 0.0f, -0.5f};
    Vec3 boundsMax{0.5f, 1.7f, 0.5f};
    std::vector<CharacterGroup> groups;
    std::unordered_map<std::string, std::vector<std::string>> typeRestrictions;
    std::unordered_map<std::string, TraitRestriction> traitRestrictions;

    Vec3 localOffset() const
    {
        return {-(boundsMin.x + boundsMax.x) * 0.5f, -boundsMin.y,
                -(boundsMin.z + boundsMax.z) * 0.5f};
    }
    float height() const { return boundsMax.y - boundsMin.y; }
};

class CharacterCatalog
{
public:
    bool load(const std::filesystem::path& path, std::string& error);
    const std::vector<CharacterDefinition>& characters() const { return characters_; }

private:
    std::vector<CharacterDefinition> characters_;
};

} // namespace m3d
