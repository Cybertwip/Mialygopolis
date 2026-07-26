#include "voxel_world.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <stdexcept>

namespace m3d {

std::string_view worldName(WorldTheme theme)
{
    return theme == WorldTheme::GreekHeaven ? "CIELO GRIEGO" : "CIUDAD ROMANA";
}

std::string_view worldDescription(WorldTheme theme)
{
    return theme == WorldTheme::GreekHeaven
        ? "TEMPLOS, JARDINES E ISLAS DEL OLIMPO"
        : "FORO, TEMPLO, ACUEDUCTO Y VILLAS";
}

Vec3 VoxelWorld::skyColor() const
{
    return theme_ == WorldTheme::GreekHeaven
        ? Vec3{0.34f, 0.66f, 0.70f}
        : Vec3{0.20f, 0.43f, 0.72f};
}

void VoxelWorld::setBox(int x0, int y0, int z0, int x1, int y1, int z1, Cubiquity::MaterialId material)
{
    if (x0 > x1) std::swap(x0, x1);
    if (y0 > y1) std::swap(y0, y1);
    if (z0 > z1) std::swap(z0, z1);
    for (int z = z0; z <= z1; ++z)
        for (int y = y0; y <= y1; ++y)
            for (int x = x0; x <= x1; ++x)
                volume_.setVoxel(x, y, z, material);
}

void VoxelWorld::setHollowBox(int x0, int y0, int z0, int x1, int y1, int z1, Cubiquity::MaterialId material)
{
    setBox(x0, y0, z0, x1, y1, z0, material);
    setBox(x0, y0, z1, x1, y1, z1, material);
    setBox(x0, y0, z0, x0, y1, z1, material);
    setBox(x1, y0, z0, x1, y1, z1, material);
    setBox(x0, y1, z0, x1, y1, z1, material);
}

void VoxelWorld::setRomanColumn(int x, int y, int z, int height, Cubiquity::MaterialId material)
{
    setBox(x - 1, y, z - 1, x + 1, y, z + 1, material);
    setBox(x, y + 1, z, x, y + height - 2, z, material);
    setBox(x - 1, y + height - 1, z - 1, x + 1, y + height, z + 1, material);
}

void VoxelWorld::setCypress(int x, int z, int height)
{
    setBox(x, 1, z, x, std::max(2, height / 3), z, 7);
    for (int layer = 0; layer < height; ++layer) {
        const int radius = layer < height / 3 ? 2 : (layer < (height * 2) / 3 ? 1 : 0);
        setBox(x - radius, 2 + layer, z - radius, x + radius, 2 + layer, z + radius, 11);
    }
}

void VoxelWorld::setTree(int x, int z, int height, int crownRadius, Cubiquity::MaterialId foliage)
{
    setBox(x, 1, z, x, height, z, 7);
    const int crownY = height + 1;
    for (int dy = -crownRadius; dy <= crownRadius; ++dy) {
        const int radius = std::max(1, crownRadius - std::abs(dy) / 2);
        setBox(x - radius, crownY + dy, z - radius, x + radius, crownY + dy, z + radius, foliage);
    }
}

void VoxelWorld::setSteppedRoof(int x0, int z0, int x1, int z1, int baseY, int layers, Cubiquity::MaterialId material)
{
    for (int layer = 0; layer < layers; ++layer) {
        if (x0 + layer > x1 - layer) break;
        setBox(x0 + layer, baseY + layer, z0, x1 - layer, baseY + layer, z1, material);
    }
}

void VoxelWorld::generateRomanCity()
{
    constexpr int half = 54;
    for (int z = -half; z <= half; ++z) {
        for (int x = -half; x <= half; ++x) {
            Cubiquity::MaterialId surface = 1; // piedra caliza
            if (std::abs(x) <= 19 && std::abs(z) <= 19) surface = 2; // mármol del foro
            if (std::abs(x) <= 3 || std::abs(z) <= 3) surface = 3; // calzada
            volume_.setVoxel(x, -2, z, 9);
            volume_.setVoxel(x, -1, z, 9);
            volume_.setVoxel(x, 0, z, surface);
        }
    }

    // Murallas, puertas y torres.
    setBox(-54, 1, -54, -51, 8, -7, 9); setBox(-54, 1, 7, -51, 8, 54, 9);
    setBox(51, 1, -54, 54, 8, -7, 9);   setBox(51, 1, 7, 54, 8, 54, 9);
    setBox(-54, 1, -54, -7, 8, -51, 9); setBox(7, 1, -54, 54, 8, -51, 9);
    setBox(-54, 1, 51, -7, 8, 54, 9);   setBox(7, 1, 51, 54, 8, 54, 9);
    for (const auto [x, z] : std::array<std::pair<int, int>, 4>{{{-51,-51},{51,-51},{-51,51},{51,51}}}) {
        setBox(x - 4, 1, z - 4, x + 4, 14, z + 4, 9);
        setBox(x - 5, 14, z - 5, x + 5, 15, z + 5, 5);
    }
    for (int coordinate = -47; coordinate <= 47; coordinate += 6) {
        if (std::abs(coordinate) > 7) {
            setBox(coordinate, 9, -54, coordinate + 2, 10, -51, 14);
            setBox(coordinate, 9, 51, coordinate + 2, 10, 54, 14);
            setBox(-54, 9, coordinate, -51, 10, coordinate + 2, 14);
            setBox(51, 9, coordinate, 54, 10, coordinate + 2, 14);
        }
    }

    // Templo principal de la colina capitolina.
    setBox(-17, 1, -47, 17, 3, -25, 2);
    for (int step = 0; step < 5; ++step)
        setBox(-12 - step, 1 + step, -24 + step, 12 + step, 1 + step, -20 + step, 2);
    for (const int x : {-14, -9, -5, 5, 9, 14}) setRomanColumn(x, 4, -25, 13, 14);
    for (int z = -42; z <= -30; z += 6) {
        setRomanColumn(-13, 4, z, 13, 14);
        setRomanColumn(13, 4, z, 13, 14);
    }
    setHollowBox(-9, 4, -44, 9, 15, -30, 14);
    setSteppedRoof(-12, -46, 12, -28, 16, 6, 5);
    setBox(-2, 4, -39, 2, 11, -35, 8); // estatua dorada

    // Foro abierto, fuente lateral, columnas conmemorativas y arboleda.
    // El eje central queda libre para el personaje y la cámara del selector.
    setBox(-16, 1, -4, -8, 1, 4, 14);
    setBox(-15, 2, -3, -9, 2, 3, 4);
    setRomanColumn(13, 2, 12, 12, 14);
    setBox(12, 14, 11, 14, 17, 13, 8);
    for (const int z : {-16, -9, 9, 16}) {
        setRomanColumn(-18, 1, z, 10, 14);
        setRomanColumn(18, 1, z, 10, 14);
    }
    for (const auto [x, z] : std::array<std::pair<int, int>, 8>{{{-22,-18},{22,-18},{-22,-7},{22,-7},{-22,7},{22,7},{-22,18},{22,18}}})
        setCypress(x, z, 12);

    // Basílica occidental.
    setBox(-48, 1, -19, -27, 2, 19, 2);
    setHollowBox(-47, 3, -18, -28, 12, 18, 14);
    for (int z = -14; z <= 14; z += 7) setRomanColumn(-26, 3, z, 10, 14);
    setSteppedRoof(-49, -20, -26, 20, 13, 5, 5);

    // Villas y mercado oriental.
    for (int z = -42; z <= 34; z += 19) {
        setHollowBox(28, 1, z, 45, 9, z + 14, 1);
        setSteppedRoof(27, z - 1, 46, z + 15, 10, 5, 5);
        setBox(31, 1, z + 3, 35, 4, z + 7, 7);
    }
    for (int z = -15; z <= 15; z += 10) {
        setBox(23, 1, z - 3, 26, 5, z + 3, 5);
        setBox(24, 6, z - 2, 25, 8, z + 2, 15);
    }

    // Acueducto meridional con arcos sugeridos por pilares separados.
    for (int x = -46; x <= 46; x += 8) {
        setBox(x - 1, 1, 37, x + 1, 13, 40, 9);
        setBox(x - 2, 12, 36, x + 2, 14, 41, 14);
    }
    setBox(-48, 15, 37, 48, 17, 40, 14);
    setBox(-47, 18, 38, 47, 18, 39, 4);

    // Puertas monumentales y antorchas.
    for (const auto [x, z] : std::array<std::pair<int, int>, 4>{{{0,-51},{0,51},{-51,0},{51,0}}}) {
        setRomanColumn(x - (z == 0 ? 0 : 5), 1, z - (x == 0 ? 0 : 5), 11, 14);
        setRomanColumn(x + (z == 0 ? 0 : 5), 1, z + (x == 0 ? 0 : 5), 11, 14);
    }
}

void VoxelWorld::generateGreekHeaven()
{
    constexpr int half = 54;
    for (int z = -half; z <= half; ++z) {
        for (int x = -half; x <= half; ++x) {
            const float radius = std::sqrt(static_cast<float>(x * x + z * z));
            int height = 0;
            if (radius > 34.0f) height = std::min(7, static_cast<int>((radius - 34.0f) / 3.0f));
            height += (radius > 39.0f) ? static_cast<int>((std::sin(x * 0.27f) + std::cos(z * 0.21f) + 2.0f) * 0.7f) : 0;
            for (int y = -2; y <= height; ++y) volume_.setVoxel(x, y, z, y == height ? 12 : 11);
            if (std::abs(x) <= 2 || std::abs(z) <= 2) volume_.setVoxel(x, height, z, 1);
        }
    }

    // Río central, estanque y cascada del acantilado.
    for (int z = 8; z <= 48; ++z) {
        const int x = static_cast<int>(std::sin(z * 0.25f) * 5.0f) - 15;
        setBox(x - 2, 1, z, x + 2, 1, z + 1, 13);
    }
    setBox(-24, 1, 30, -7, 1, 48, 13);
    setBox(-22, 2, 43, -9, 11, 49, 9);
    setBox(-17, 3, 42, -14, 12, 43, 13);

    // Templo blanco del Olimpo.
    setBox(-13, 1, -35, 13, 2, -18, 14);
    for (int x = -10; x <= 10; x += 5) {
        setRomanColumn(x, 3, -20, 11, 14);
        setRomanColumn(x, 3, -33, 11, 14);
    }
    setSteppedRoof(-14, -36, 14, -17, 14, 6, 14);
    setBox(-2, 3, -29, 2, 9, -25, 8);

    // Bosques de laureles, flores y senderos.
    const std::array<std::pair<int, int>, 22> trees{{
        {-35,-30},{-28,-18},{-39,-5},{-30,9},{-40,22},{-29,35},
        {-12,-45},{8,-44},{24,-38},{38,-28},{30,-13},{42,0},
        {31,14},{41,29},{27,40},{12,44},{-2,38},{-34,44},
        {-20,18},{19,22},{-22,-2},{22,-4}
    }};
    int treeIndex = 0;
    for (const auto [x, z] : trees) {
        setTree(x, z, 7 + (treeIndex % 5), 3 + (treeIndex % 2), treeIndex % 3 == 0 ? 6 : 11);
        ++treeIndex;
    }
    for (int x = -28; x <= 28; x += 7) {
        setBox(x, 1, 9, x + 2, 1, 11, 10);
        setBox(x + 3, 1, -11, x + 5, 1, -9, 8);
    }

    // Arcos naturales y santuarios de piedra.
    for (const int x : {-30, 30}) {
        setBox(x - 4, 1, -2, x - 2, 10, 2, 9);
        setBox(x + 2, 1, -2, x + 4, 10, 2, 9);
        setBox(x - 4, 9, -2, x + 4, 12, 2, 14);
        setBox(x - 1, 11, -1, x + 1, 14, 1, 10);
    }

    // Islas del Olimpo, nubes de mármol y cortinas de agua.
    for (const auto [x, z] : std::array<std::pair<int, int>, 3>{{{-36,-22},{34,-18},{7,35}}}) {
        setBox(x - 8, 17, z - 6, x + 8, 18, z + 6, 11);
        setBox(x - 6, 19, z - 5, x + 6, 20, z + 5, 12);
        setBox(x - 3, 14, z - 2, x + 3, 16, z + 2, 9);
        setTree(x, z, 6, 3, 6);
        setBox(x + 5, 2, z, x + 7, 18, z + 1, 13);
    }
    for (const auto [x, y, z] : std::array<std::array<int, 3>, 6>{{
        std::array<int,3>{-44,14,8}, {-25,20,-38}, {-5,15,48},
        {24,17,32}, {43,13,8}, {18,22,-42}
    }}) {
        setBox(x - 7, y, z - 3, x + 7, y + 1, z + 3, 14);
        setBox(x - 4, y + 2, z - 4, x + 4, y + 3, z + 4, 14);
    }

    // Portal luminoso al fondo.
    setRomanColumn(0, 1, 47, 13, 14);
    setRomanColumn(10, 1, 47, 13, 14);
    setBox(0, 13, 46, 10, 16, 48, 10);
    setBox(3, 2, 46, 7, 12, 48, 13);
}

bool VoxelWorld::isCollidable(Cubiquity::MaterialId material) const
{
    // Agua, flores y efectos luminosos no bloquean al jugador.
    return material != 0 && material != 4 && material != 10 && material != 13;
}

bool VoxelWorld::collidesCapsule(const Vec3& position, float radius, float height,
                                 std::span<const DynamicCollider> dynamicColliders) const
{
    const float playerMinY = position.y + 0.02f;
    const float playerMaxY = position.y + height;
    const int minX = static_cast<int>(std::floor((position.x - radius) / VoxelScale));
    const int maxX = static_cast<int>(std::floor((position.x + radius) / VoxelScale));
    const int minZ = static_cast<int>(std::floor((position.z - radius) / VoxelScale));
    const int maxZ = static_cast<int>(std::floor((position.z + radius) / VoxelScale));
    const int minY = static_cast<int>(std::floor(playerMinY / VoxelScale)) - 1;
    const int maxY = static_cast<int>(std::floor(playerMaxY / VoxelScale)) + 1;
    const float half = VoxelScale * 0.5f;

    for (int z = minZ; z <= maxZ; ++z) {
        for (int y = minY; y <= maxY; ++y) {
            for (int x = minX; x <= maxX; ++x) {
                if (!isCollidable(volume_.voxel(x, y, z))) continue;
                const float blockMinY = y * VoxelScale - half;
                const float blockMaxY = y * VoxelScale + half;
                if (playerMaxY <= blockMinY || playerMinY >= blockMaxY) continue;
                const float blockMinX = x * VoxelScale - half;
                const float blockMaxX = x * VoxelScale + half;
                const float blockMinZ = z * VoxelScale - half;
                const float blockMaxZ = z * VoxelScale + half;
                const float closestX = std::clamp(position.x, blockMinX, blockMaxX);
                const float closestZ = std::clamp(position.z, blockMinZ, blockMaxZ);
                const float dx = position.x - closestX;
                const float dz = position.z - closestZ;
                if (dx * dx + dz * dz < radius * radius) return true;
            }
        }
    }

    for (const auto& collider : dynamicColliders) {
        const float colliderMinY = collider.position.y;
        const float colliderMaxY = collider.position.y + collider.height;
        if (playerMaxY <= colliderMinY || playerMinY >= colliderMaxY) continue;
        const float dx = position.x - collider.position.x;
        const float dz = position.z - collider.position.z;
        const float combined = radius + collider.radius;
        if (dx * dx + dz * dz < combined * combined) return true;
    }
    return false;
}

float VoxelWorld::groundHeight(const Vec3& position, float maxStep) const
{
    const int x = static_cast<int>(std::round(position.x / VoxelScale));
    const int z = static_cast<int>(std::round(position.z / VoxelScale));
    const int currentY = static_cast<int>(std::floor(position.y / VoxelScale));
    const int stepVoxels = std::max(1, static_cast<int>(std::ceil(maxStep / VoxelScale)));
    for (int y = currentY + stepVoxels; y >= currentY - 8; --y) {
        if (isCollidable(volume_.voxel(x, y, z))) {
            return (static_cast<float>(y) + 0.5f) * VoxelScale + 0.005f;
        }
    }
    return position.y;
}

Vec3 VoxelWorld::resolvePlayerMotion(const Vec3& from, const Vec3& proposed,
                                     float radius, float height,
                                     std::span<const DynamicCollider> dynamicColliders) const
{
    Vec3 result = from;
    Vec3 candidate = result;
    candidate.x = proposed.x;
    if (!collidesCapsule(candidate, radius, height, dynamicColliders)) result.x = candidate.x;

    candidate = result;
    candidate.z = proposed.z;
    if (!collidesCapsule(candidate, radius, height, dynamicColliders)) result.z = candidate.z;

    const float floor = groundHeight(result);
    if (std::abs(floor - result.y) <= 0.55f) result.y = floor;
    return result;
}

void VoxelWorld::finishGeneration()
{
    volume_.bake();
    constexpr Cubiquity::u32 maxGlyphs = 500000;
    glyphs_.resize(maxGlyphs);
    const auto count = Cubiquity::extractGlyphs(volume_, false, glyphs_.data(), maxGlyphs);
    if (count >= maxGlyphs) throw std::runtime_error("El mundo vóxel superó el límite de glifos");
    glyphs_.resize(count);
    std::cout << worldName(theme_) << ": " << nodeCount() << " nodos DAG, "
              << glyphs_.size() << " glifos de renderizado\n";
}

void VoxelWorld::generate(WorldTheme theme)
{
    theme_ = theme;
    volume_.fill(0);
    glyphs_.clear();
    if (theme == WorldTheme::GreekHeaven) generateGreekHeaven();
    else generateRomanCity();
    finishGeneration();
}

} // namespace m3d
