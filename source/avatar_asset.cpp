#include "avatar_asset.h"

#include "stb_image.h"

#include <algorithm>
#include <cstddef>
#include <cstring>
#include <fstream>
#include <limits>

namespace m3d {
namespace {

constexpr std::uint32_t AssetVersion = 2;
constexpr std::uint32_t MaxReasonableCount = 50'000'000;

bool readExact(std::istream& input, void* destination, std::size_t size)
{
    if (size == 0) return true;
    input.read(static_cast<char*>(destination), static_cast<std::streamsize>(size));
    return input.gcount() == static_cast<std::streamsize>(size);
}

template <typename T>
bool readValue(std::istream& input, T& value)
{
    return readExact(input, &value, sizeof(T));
}

void configureTexture(GLuint texture, int width, int height, const unsigned char* pixels)
{
    glBindTexture(GL_TEXTURE_2D, texture);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
}

} // namespace

AvatarAsset::~AvatarAsset() { releaseGpu(); }

bool AvatarAsset::load(const std::filesystem::path& path, std::string& error)
{
    releaseGpu();
    primitives_.clear();
    vertexCount_ = 0;
    triangleCount_ = 0;
    path_ = path;
    clearTint();

    std::ifstream input(path, std::ios::binary);
    if (!input) {
        error = "No se pudo abrir el rasgo del avatar: " + path.string();
        return false;
    }

    char magic[4]{};
    std::uint32_t version = 0;
    std::uint32_t primitiveCount = 0;
    std::uint32_t reserved = 0;
    if (!readExact(input, magic, sizeof(magic)) || !readValue(input, version) ||
        !readValue(input, primitiveCount) || !readValue(input, reserved) ||
        std::memcmp(magic, "M3A2", 4) != 0 || version != AssetVersion || primitiveCount > 4096) {
        error = "El rasgo del avatar tiene un formato no compatible: " + path.string();
        return false;
    }
    (void)reserved;

    boundsMin_ = Vec3{std::numeric_limits<float>::max()};
    boundsMax_ = Vec3{std::numeric_limits<float>::lowest()};
    primitives_.reserve(primitiveCount);
    for (std::uint32_t primitiveIndex = 0; primitiveIndex < primitiveCount; ++primitiveIndex) {
        std::uint32_t vertexCount = 0, indexCount = 0, imageSize = 0, flags = 0, meshSlot = 0, labelSize = 0;
        std::array<float, 4> baseColor{};
        float alphaCutoff = 0.5f;
        if (!readValue(input, vertexCount) || !readValue(input, indexCount) ||
            !readValue(input, imageSize) || !readValue(input, flags) || !readValue(input, meshSlot) ||
            !readExact(input, baseColor.data(), sizeof(float) * baseColor.size()) ||
            !readValue(input, alphaCutoff) || !readValue(input, labelSize) ||
            vertexCount == 0 || indexCount == 0 || vertexCount > MaxReasonableCount ||
            indexCount > MaxReasonableCount || imageSize > 128U * 1024U * 1024U || labelSize > 4096) {
            error = "La cabecera de la primitiva del avatar no es válida: " + path.string();
            return false;
        }

        AvatarPrimitive primitive;
        primitive.flags = flags;
        primitive.meshSlot = meshSlot;
        primitive.baseColor = baseColor;
        primitive.alphaCutoff = alphaCutoff;
        primitive.label.resize(labelSize);
        primitive.vertices.resize(vertexCount);
        primitive.indices.resize(indexCount);
        primitive.imageBytes.resize(imageSize);
        if (!readExact(input, primitive.label.data(), labelSize) ||
            !readExact(input, primitive.vertices.data(), primitive.vertices.size() * sizeof(AvatarVertex)) ||
            !readExact(input, primitive.indices.data(), primitive.indices.size() * sizeof(std::uint32_t)) ||
            !readExact(input, primitive.imageBytes.data(), primitive.imageBytes.size())) {
            error = "Los datos de la primitiva del avatar están truncados: " + path.string();
            return false;
        }
        for (const auto& vertex : primitive.vertices) {
            boundsMin_.x = std::min(boundsMin_.x, vertex.position[0]);
            boundsMin_.y = std::min(boundsMin_.y, vertex.position[1]);
            boundsMin_.z = std::min(boundsMin_.z, vertex.position[2]);
            boundsMax_.x = std::max(boundsMax_.x, vertex.position[0]);
            boundsMax_.y = std::max(boundsMax_.y, vertex.position[1]);
            boundsMax_.z = std::max(boundsMax_.z, vertex.position[2]);
        }
        vertexCount_ += primitive.vertices.size();
        triangleCount_ += primitive.indices.size() / 3;
        primitives_.push_back(std::move(primitive));
    }
    return !primitives_.empty();
}

GLuint AvatarAsset::loadTextureBytes(const unsigned char* bytes, int size, std::string& error)
{
    int width = 0, height = 0, channels = 0;
    unsigned char* pixels = stbi_load_from_memory(bytes, size, &width, &height, &channels, STBI_rgb_alpha);
    if (!pixels) {
        error = std::string("No se pudo decodificar la textura: ") + stbi_failure_reason();
        return 0;
    }
    GLuint texture = 0;
    glGenTextures(1, &texture);
    configureTexture(texture, width, height, pixels);
    stbi_image_free(pixels);
    return texture;
}

GLuint AvatarAsset::loadExternalTexture(const std::filesystem::path& path, std::string& error)
{
    int width = 0, height = 0, channels = 0;
    unsigned char* pixels = stbi_load(path.string().c_str(), &width, &height, &channels, STBI_rgb_alpha);
    if (!pixels) {
        error = "No se pudo cargar la textura " + path.string() + ": " + stbi_failure_reason();
        return 0;
    }
    GLuint texture = 0;
    glGenTextures(1, &texture);
    configureTexture(texture, width, height, pixels);
    stbi_image_free(pixels);
    return texture;
}

bool AvatarAsset::upload(std::string& error)
{
    stbi_set_flip_vertically_on_load(0);
    for (auto& primitive : primitives_) {
        glGenVertexArrays(1, &primitive.vao);
        glGenBuffers(1, &primitive.vertexBuffer);
        glGenBuffers(1, &primitive.indexBuffer);
        glBindVertexArray(primitive.vao);
        glBindBuffer(GL_ARRAY_BUFFER, primitive.vertexBuffer);
        glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(primitive.vertices.size() * sizeof(AvatarVertex)),
                     primitive.vertices.data(), GL_STATIC_DRAW);
        primitive.indexCount = static_cast<GLsizei>(primitive.indices.size());
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, primitive.indexBuffer);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLsizeiptr>(primitive.indices.size() * sizeof(std::uint32_t)),
                     primitive.indices.data(), GL_STATIC_DRAW);

        const auto attribute = [](GLuint index, GLint size, std::size_t offset) {
            glEnableVertexAttribArray(index);
            glVertexAttribPointer(index, size, GL_FLOAT, GL_FALSE, sizeof(AvatarVertex), reinterpret_cast<void*>(offset));
        };
        attribute(0, 3, offsetof(AvatarVertex, position));
        attribute(1, 3, offsetof(AvatarVertex, normal));
        attribute(2, 2, offsetof(AvatarVertex, uv));
        attribute(3, 3, offsetof(AvatarVertex, walkAPosition));
        attribute(4, 3, offsetof(AvatarVertex, walkANormal));
        attribute(5, 3, offsetof(AvatarVertex, walkBPosition));
        attribute(6, 3, offsetof(AvatarVertex, walkBNormal));
        attribute(7, 3, offsetof(AvatarVertex, runAPosition));
        attribute(8, 3, offsetof(AvatarVertex, runANormal));
        attribute(9, 3, offsetof(AvatarVertex, runBPosition));
        attribute(10, 3, offsetof(AvatarVertex, runBNormal));
        attribute(11, 3, offsetof(AvatarVertex, wavePosition));
        attribute(12, 3, offsetof(AvatarVertex, waveNormal));
        attribute(13, 3, offsetof(AvatarVertex, cheerPosition));
        attribute(14, 3, offsetof(AvatarVertex, cheerNormal));
        attribute(15, 3, offsetof(AvatarVertex, dancePosition));

        if (!primitive.imageBytes.empty()) {
            primitive.texture = loadTextureBytes(primitive.imageBytes.data(), static_cast<int>(primitive.imageBytes.size()), error);
            if (!primitive.texture) return false;
        }
    }
    glBindVertexArray(0);
    return true;
}

bool AvatarAsset::applyTextureVariant(const std::vector<std::filesystem::path>& paths, std::string& error)
{
    clearVariant();
    for (const auto& path : paths) {
        const GLuint texture = loadExternalTexture(path, error);
        if (!texture) {
            clearVariant();
            return false;
        }
        variantTextures_.push_back(texture);
    }
    return true;
}

void AvatarAsset::clearVariant()
{
    for (const GLuint texture : variantTextures_) glDeleteTextures(1, &texture);
    variantTextures_.clear();
}

GLuint AvatarAsset::textureFor(const AvatarPrimitive& primitive) const
{
    if (variantTextures_.empty()) return primitive.texture;
    const std::size_t slot = std::min<std::size_t>(primitive.meshSlot, variantTextures_.size() - 1);
    return variantTextures_[slot];
}

void AvatarAsset::discardCpuData()
{
    for (auto& primitive : primitives_) {
        primitive.vertices.clear();
        primitive.vertices.shrink_to_fit();
        primitive.indices.clear();
        primitive.indices.shrink_to_fit();
        primitive.imageBytes.clear();
        primitive.imageBytes.shrink_to_fit();
    }
}

void AvatarAsset::releaseGpu()
{
    clearVariant();
    for (auto& primitive : primitives_) {
        if (primitive.texture) glDeleteTextures(1, &primitive.texture);
        if (primitive.indexBuffer) glDeleteBuffers(1, &primitive.indexBuffer);
        if (primitive.vertexBuffer) glDeleteBuffers(1, &primitive.vertexBuffer);
        if (primitive.vao) glDeleteVertexArrays(1, &primitive.vao);
        primitive.texture = primitive.indexBuffer = primitive.vertexBuffer = primitive.vao = 0;
    }
}

std::size_t AvatarAsset::vertexCount() const
{
    return vertexCount_;
}

} // namespace m3d
