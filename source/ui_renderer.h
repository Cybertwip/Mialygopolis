#pragma once

#include "glad/glad.h"
#include "math_types.h"

#include <array>
#include <string_view>
#include <vector>

namespace m3d {

struct UiColor
{
    float r;
    float g;
    float b;
    float a;
};

class UiRenderer
{
public:
    bool initialize(std::string& error);
    void shutdown();

    void begin(int width, int height);
    void rectangle(float x, float y, float width, float height, UiColor color);
    void outline(float x, float y, float width, float height, float thickness, UiColor color);
    void text(float x, float y, float scale, std::string_view value, UiColor color);
    void textCentered(float centerX, float y, float scale, std::string_view value, UiColor color);
    float textWidth(float scale, std::string_view value) const;
    void flush();

private:
    struct Vertex
    {
        float x;
        float y;
        float r;
        float g;
        float b;
        float a;
    };

    void pushQuad(float x, float y, float width, float height, UiColor color);
    static std::array<unsigned char, 7> glyph(char32_t character);

    GLuint program_ = 0;
    GLuint vao_ = 0;
    GLuint buffer_ = 0;
    GLint viewportLocation_ = -1;
    int width_ = 1;
    int height_ = 1;
    std::vector<Vertex> vertices_;
};

} // namespace m3d
