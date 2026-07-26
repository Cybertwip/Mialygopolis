#include "character_catalog.h"

#include <array>
#include <cstring>
#include <fstream>
#include <limits>

namespace m3d {
namespace {

constexpr std::uint32_t CatalogVersion = 1;
constexpr std::uint32_t MaxCount = 100000;

bool readExact(std::istream& input, void* destination, std::size_t bytes)
{
    if (bytes == 0) return true;
    input.read(static_cast<char*>(destination), static_cast<std::streamsize>(bytes));
    return input.gcount() == static_cast<std::streamsize>(bytes);
}

template <typename T>
bool readValue(std::istream& input, T& value)
{
    return readExact(input, &value, sizeof(value));
}

bool readString(std::istream& input, std::string& value)
{
    std::uint32_t length = 0;
    if (!readValue(input, length) || length > 1024U * 1024U) return false;
    value.resize(length);
    return readExact(input, value.data(), length);
}

bool readStringVector(std::istream& input, std::vector<std::string>& values)
{
    std::uint32_t count = 0;
    if (!readValue(input, count) || count > MaxCount) return false;
    values.resize(count);
    for (auto& value : values) {
        if (!readString(input, value)) return false;
    }
    return true;
}

} // namespace

bool CharacterCatalog::load(const std::filesystem::path& path, std::string& error)
{
    characters_.clear();
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        error = "No se pudo abrir el catálogo de personajes: " + path.string();
        return false;
    }

    char magic[4]{};
    std::uint32_t version = 0;
    std::uint32_t characterCount = 0;
    if (!readExact(input, magic, sizeof(magic)) || !readValue(input, version) ||
        !readValue(input, characterCount) || std::memcmp(magic, "M3C1", 4) != 0 ||
        version != CatalogVersion || characterCount == 0 || characterCount > 128) {
        error = "La cabecera del catálogo de personajes no es válida: " + path.string();
        return false;
    }

    characters_.resize(characterCount);
    for (auto& character : characters_) {
        if (!readString(input, character.id) || !readString(input, character.name) ||
            !readString(input, character.tagline) ||
            !readExact(input, &character.accent, sizeof(float) * 3) ||
            !readExact(input, &character.boundsMin, sizeof(float) * 3) ||
            !readExact(input, &character.boundsMax, sizeof(float) * 3)) {
            error = "El catálogo está truncado al leer los datos del personaje";
            return false;
        }

        std::uint32_t groupCount = 0;
        if (!readValue(input, groupCount) || groupCount > 256) {
            error = "El catálogo tiene una cantidad de categorías no válida";
            return false;
        }
        character.groups.resize(groupCount);
        for (auto& group : character.groups) {
            std::uint8_t required = 0;
            std::array<std::uint8_t, 3> padding{};
            std::uint32_t optionCount = 0;
            if (!readString(input, group.id) || !readString(input, group.name) ||
                !readValue(input, required) || !readExact(input, padding.data(), padding.size()) ||
                !readValue(input, group.defaultOption) || !readValue(input, optionCount) || optionCount > 4096) {
                error = "El catálogo está truncado al leer una categoría de rasgos";
                return false;
            }
            group.required = required != 0;
            group.options.resize(optionCount);
            for (auto& option : group.options) {
                if (!readString(input, option.id) || !readString(input, option.name) ||
                    !readString(input, option.assetFile) || !readString(input, option.thumbnail) ||
                    !readStringVector(input, option.types)) {
                    error = "El catálogo está truncado al leer una opción de rasgo";
                    return false;
                }
                std::uint8_t variantKind = 0;
                std::array<std::uint8_t, 3> variantPadding{};
                std::uint32_t variantCount = 0;
                if (!readValue(input, variantKind) || !readExact(input, variantPadding.data(), variantPadding.size()) ||
                    !readValue(input, option.defaultVariant) || !readValue(input, variantCount) || variantCount > 10000) {
                    error = "El catálogo contiene datos de variante no válidos";
                    return false;
                }
                option.variantKind = static_cast<VariantKind>(variantKind);
                option.variants.resize(variantCount);
                for (auto& variant : option.variants) {
                    if (!readString(input, variant.id) || !readString(input, variant.name) ||
                        !readStringVector(input, variant.texturePaths)) {
                        error = "El catálogo está truncado al leer una variante";
                        return false;
                    }
                    std::uint32_t colorCount = 0;
                    if (!readValue(input, colorCount) || colorCount > 4096) {
                        error = "El catálogo tiene una cantidad de colores no válida";
                        return false;
                    }
                    variant.colors.resize(colorCount);
                    for (auto& color : variant.colors) {
                        if (!readExact(input, &color, sizeof(float) * 3)) {
                            error = "El catálogo está truncado al leer colores";
                            return false;
                        }
                    }
                }
            }
        }

        std::uint32_t typeRestrictionCount = 0;
        if (!readValue(input, typeRestrictionCount) || typeRestrictionCount > 4096) {
            error = "El catálogo contiene restricciones de tipo no válidas";
            return false;
        }
        for (std::uint32_t i = 0; i < typeRestrictionCount; ++i) {
            std::string type;
            std::vector<std::string> restricted;
            if (!readString(input, type) || !readStringVector(input, restricted)) {
                error = "El catálogo está truncado en las restricciones de tipo";
                return false;
            }
            character.typeRestrictions.emplace(std::move(type), std::move(restricted));
        }

        std::uint32_t traitRestrictionCount = 0;
        if (!readValue(input, traitRestrictionCount) || traitRestrictionCount > 4096) {
            error = "El catálogo contiene restricciones de rasgo no válidas";
            return false;
        }
        for (std::uint32_t i = 0; i < traitRestrictionCount; ++i) {
            std::string groupId;
            TraitRestriction restriction;
            if (!readString(input, groupId) || !readStringVector(input, restriction.restrictedTraits) ||
                !readStringVector(input, restriction.restrictedTypes)) {
                error = "El catálogo está truncado en las restricciones de rasgo";
                return false;
            }
            character.traitRestrictions.emplace(std::move(groupId), std::move(restriction));
        }
    }
    return true;
}

} // namespace m3d
