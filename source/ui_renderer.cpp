#include "ui_renderer.h"

#include <algorithm>
#include <cctype>
#include <iostream>
#include <string>
#include <string_view>

namespace m3d {
namespace {


std::vector<char32_t> decodeUtf8(std::string_view value)
{
    std::vector<char32_t> result;
    for (std::size_t i = 0; i < value.size();) {
        const unsigned char first = static_cast<unsigned char>(value[i]);
        if (first < 0x80) {
            result.push_back(first);
            ++i;
        } else if ((first & 0xE0) == 0xC0 && i + 1 < value.size()) {
            result.push_back(((first & 0x1F) << 6) |
                             (static_cast<unsigned char>(value[i + 1]) & 0x3F));
            i += 2;
        } else if ((first & 0xF0) == 0xE0 && i + 2 < value.size()) {
            result.push_back(((first & 0x0F) << 12) |
                             ((static_cast<unsigned char>(value[i + 1]) & 0x3F) << 6) |
                             (static_cast<unsigned char>(value[i + 2]) & 0x3F));
            i += 3;
        } else {
            result.push_back(U'?');
            ++i;
        }
    }
    return result;
}

char32_t uppercaseSpanish(char32_t value)
{
    if (value >= U'a' && value <= U'z') return value - U'a' + U'A';
    switch (value) {
    case U'á': return U'Á'; case U'é': return U'É'; case U'í': return U'Í';
    case U'ó': return U'Ó'; case U'ú': return U'Ú'; case U'ü': return U'Ü';
    case U'ñ': return U'Ñ'; default: return value;
    }
}

GLuint compileShader(GLenum type, const char* source, std::string& error)
{
    const GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);
    GLint ok = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        GLint length = 0;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &length);
        std::string log(static_cast<std::size_t>(std::max(1, length)), '\0');
        glGetShaderInfoLog(shader, length, nullptr, log.data());
        error = log;
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

} // namespace

bool UiRenderer::initialize(std::string& error)
{
    static constexpr const char* vertexSource = R"GLSL(
#version 330 core
layout(location = 0) in vec2 a_position;
layout(location = 1) in vec4 a_color;
uniform vec2 u_viewport;
out vec4 v_color;
void main()
{
    vec2 ndc = vec2(a_position.x / u_viewport.x * 2.0 - 1.0,
                    1.0 - a_position.y / u_viewport.y * 2.0);
    gl_Position = vec4(ndc, 0.0, 1.0);
    v_color = a_color;
}
)GLSL";
    static constexpr const char* fragmentSource = R"GLSL(
#version 330 core
in vec4 v_color;
out vec4 out_color;
void main() { out_color = v_color; }
)GLSL";

    const GLuint vertex = compileShader(GL_VERTEX_SHADER, vertexSource, error);
    if (!vertex) return false;
    const GLuint fragment = compileShader(GL_FRAGMENT_SHADER, fragmentSource, error);
    if (!fragment) {
        glDeleteShader(vertex);
        return false;
    }
    program_ = glCreateProgram();
    glAttachShader(program_, vertex);
    glAttachShader(program_, fragment);
    glLinkProgram(program_);
    glDeleteShader(vertex);
    glDeleteShader(fragment);
    GLint ok = GL_FALSE;
    glGetProgramiv(program_, GL_LINK_STATUS, &ok);
    if (!ok) {
        GLint length = 0;
        glGetProgramiv(program_, GL_INFO_LOG_LENGTH, &length);
        std::string log(static_cast<std::size_t>(std::max(1, length)), '\0');
        glGetProgramInfoLog(program_, length, nullptr, log.data());
        error = log;
        shutdown();
        return false;
    }

    viewportLocation_ = glGetUniformLocation(program_, "u_viewport");
    glGenVertexArrays(1, &vao_);
    glGenBuffers(1, &buffer_);
    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, buffer_);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(0));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(sizeof(float) * 2));
    glBindVertexArray(0);
    vertices_.reserve(64 * 1024);
    return true;
}

void UiRenderer::shutdown()
{
    if (buffer_) glDeleteBuffers(1, &buffer_);
    if (vao_) glDeleteVertexArrays(1, &vao_);
    if (program_) glDeleteProgram(program_);
    buffer_ = 0;
    vao_ = 0;
    program_ = 0;
}

void UiRenderer::begin(int width, int height)
{
    width_ = std::max(1, width);
    height_ = std::max(1, height);
    vertices_.clear();
}

void UiRenderer::pushQuad(float x, float y, float width, float height, UiColor color)
{
    const Vertex a{x, y, color.r, color.g, color.b, color.a};
    const Vertex b{x + width, y, color.r, color.g, color.b, color.a};
    const Vertex c{x + width, y + height, color.r, color.g, color.b, color.a};
    const Vertex d{x, y + height, color.r, color.g, color.b, color.a};
    vertices_.insert(vertices_.end(), {a, b, c, a, c, d});
}

void UiRenderer::rectangle(float x, float y, float width, float height, UiColor color)
{
    pushQuad(x, y, width, height, color);
}

void UiRenderer::outline(float x, float y, float width, float height, float thickness, UiColor color)
{
    rectangle(x, y, width, thickness, color);
    rectangle(x, y + height - thickness, width, thickness, color);
    rectangle(x, y + thickness, thickness, height - thickness * 2.0f, color);
    rectangle(x + width - thickness, y + thickness, thickness, height - thickness * 2.0f, color);
}

float UiRenderer::textWidth(float scale, std::string_view value) const
{
    float current = 0.0f;
    float widest = 0.0f;
    for (char32_t character : decodeUtf8(value)) {
        if (character == U'\n') {
            widest = std::max(widest, current);
            current = 0.0f;
        } else {
            current += 6.0f * scale;
        }
    }
    return std::max(widest, current > 0.0f ? current - scale : current);
}

void UiRenderer::text(float x, float y, float scale, std::string_view value, UiColor color)
{
    const float startX = x;
    for (char32_t character : decodeUtf8(value)) {
        if (character == U'\n') {
            x = startX;
            y += 9.0f * scale;
            continue;
        }
        const auto rows = glyph(uppercaseSpanish(character));
        for (int row = 0; row < 7; ++row) {
            for (int column = 0; column < 5; ++column) {
                if (rows[static_cast<std::size_t>(row)] & (1U << (4 - column))) {
                    pushQuad(x + static_cast<float>(column) * scale,
                             y + static_cast<float>(row) * scale,
                             scale, scale, color);
                }
            }
        }
        x += 6.0f * scale;
    }
}

void UiRenderer::textCentered(float centerX, float y, float scale, std::string_view value, UiColor color)
{
    text(centerX - textWidth(scale, value) * 0.5f, y, scale, value, color);
}

void UiRenderer::flush()
{
    if (vertices_.empty()) return;
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glUseProgram(program_);
    glUniform2f(viewportLocation_, static_cast<float>(width_), static_cast<float>(height_));
    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, buffer_);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(vertices_.size() * sizeof(Vertex)),
                 vertices_.data(), GL_STREAM_DRAW);
    glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(vertices_.size()));
    glBindVertexArray(0);
    glDisable(GL_BLEND);
}

std::array<unsigned char, 7> UiRenderer::glyph(char32_t c)
{
    switch (c) {
    case 'A': return {14,17,17,31,17,17,17};
    case 'B': return {30,17,17,30,17,17,30};
    case 'C': return {14,17,16,16,16,17,14};
    case 'D': return {30,17,17,17,17,17,30};
    case 'E': return {31,16,16,30,16,16,31};
    case 'F': return {31,16,16,30,16,16,16};
    case 'G': return {14,17,16,23,17,17,15};
    case 'H': return {17,17,17,31,17,17,17};
    case 'I': return {31,4,4,4,4,4,31};
    case 'J': return {7,2,2,2,18,18,12};
    case 'K': return {17,18,20,24,20,18,17};
    case 'L': return {16,16,16,16,16,16,31};
    case 'M': return {17,27,21,21,17,17,17};
    case 'N': return {17,25,21,19,17,17,17};
    case 'O': return {14,17,17,17,17,17,14};
    case 'P': return {30,17,17,30,16,16,16};
    case 'Q': return {14,17,17,17,21,18,13};
    case 'R': return {30,17,17,30,20,18,17};
    case 'S': return {15,16,16,14,1,1,30};
    case 'T': return {31,4,4,4,4,4,4};
    case 'U': return {17,17,17,17,17,17,14};
    case 'V': return {17,17,17,17,17,10,4};
    case 'W': return {17,17,17,21,21,21,10};
    case 'X': return {17,17,10,4,10,17,17};
    case 'Y': return {17,17,10,4,4,4,4};
    case 'Z': return {31,1,2,4,8,16,31};
    case '0': return {14,17,19,21,25,17,14};
    case '1': return {4,12,4,4,4,4,14};
    case '2': return {14,17,1,2,4,8,31};
    case '3': return {30,1,1,14,1,1,30};
    case '4': return {2,6,10,18,31,2,2};
    case '5': return {31,16,16,30,1,1,30};
    case '6': return {14,16,16,30,17,17,14};
    case '7': return {31,1,2,4,8,8,8};
    case '8': return {14,17,17,14,17,17,14};
    case '9': return {14,17,17,15,1,1,14};
    case '-': return {0,0,0,31,0,0,0};
    case '_': return {0,0,0,0,0,0,31};
    case ':': return {0,4,4,0,4,4,0};
    case '.': return {0,0,0,0,0,6,6};
    case ',': return {0,0,0,0,4,4,8};
    case '/': return {1,2,2,4,8,8,16};
    case '>': return {16,8,4,2,4,8,16};
    case '<': return {1,2,4,8,4,2,1};
    case '[': return {14,8,8,8,8,8,14};
    case ']': return {14,2,2,2,2,2,14};
    case '+': return {0,4,4,31,4,4,0};
    case '!': return {4,4,4,4,4,0,4};
    case '?': return {14,17,1,2,4,0,4};
    case U'Á': return {4,14,17,31,17,17,17};
    case U'É': return {4,31,16,30,16,16,31};
    case U'Í': return {4,14,4,4,4,4,14};
    case U'Ó': return {4,14,17,17,17,17,14};
    case U'Ú': return {4,17,17,17,17,17,14};
    case U'Ü': return {10,17,17,17,17,17,14};
    case U'Ñ': return {10,21,25,21,19,17,17};
    case U'¿': return {4,0,4,8,16,17,14};
    case U'¡': return {4,0,4,4,4,4,4};
    case ' ': return {0,0,0,0,0,0,0};
    default:  return {14,17,1,2,4,0,4};
    }
}

} // namespace m3d
