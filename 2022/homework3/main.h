#ifdef WIN32
#include <SDL.h>
#undef main
#else

#include <SDL2/SDL.h>

#endif

#include <GL/glew.h>

#include <stdexcept>
#include <iostream>
#include <fstream>
#include <vector>
#include <map>


std::string to_string(std::string_view str) {
    return std::string(str.begin(), str.end());
}

void sdl2_fail(std::string_view message) {
    throw std::runtime_error(to_string(message) + SDL_GetError());
}

void glew_fail(std::string_view message, GLenum error) {
    throw std::runtime_error(to_string(message) + reinterpret_cast<const char *>(glewGetErrorString(error)));
}

struct vertex {
    glm::vec3 position;
    glm::vec3 tangent;
    glm::vec3 normal;
    glm::vec2 texcoord;
};

std::pair<std::vector<vertex>, std::vector<std::uint32_t>> generate_sphere(float radius,
                                                                           int quality,
                                                                           bool hemisphere = false) {
    std::vector<vertex> vertices;

    for (int latitude = -quality; latitude <= (hemisphere ? 0 : quality); ++latitude) {
        for (int longitude = 0; longitude <= 4 * quality; ++longitude) {
            float lat = (latitude * glm::pi<float>()) / (2.f * quality);
            float lon = (longitude * glm::pi<float>()) / (2.f * quality);

            auto &vertex = vertices.emplace_back();
            vertex.normal = {std::cos(lat) * std::cos(lon), std::sin(lat), std::cos(lat) * std::sin(lon)};
            vertex.position = vertex.normal * radius;
            vertex.tangent = {-std::cos(lat) * std::sin(lon), 0.f, std::cos(lat) * std::cos(lon)};
            vertex.texcoord.x = (longitude * 1.f) / (4.f * quality);
            vertex.texcoord.y = (latitude * 1.f) / (2.f * quality) + 0.5f;
        }
    }
    auto &centre = vertices.emplace_back();
    centre.normal = {0.f, 1.f, 0.f};
    centre.position = glm::vec3(0.f);
    centre.tangent = glm::vec3(0.f);
    centre.texcoord.x = quality / (4.f * quality);
    centre.texcoord.y = quality / (4.f * quality);
    std::uint32_t centre_index = vertices.size() - 1;

    std::vector<std::uint32_t> indices;

    for (int latitude = 0; latitude < (hemisphere ? 1 : 2) * quality; ++latitude) {
        for (int longitude = 0; longitude < 4 * quality; ++longitude) {
            std::uint32_t i0 = (latitude + 0) * (4 * quality + 1) + (longitude + 0);
            std::uint32_t i1 = (latitude + 1) * (4 * quality + 1) + (longitude + 0);
            std::uint32_t i2 = (latitude + 0) * (4 * quality + 1) + (longitude + 1);
            std::uint32_t i3 = (latitude + 1) * (4 * quality + 1) + (longitude + 1);

            indices.insert(indices.end(), {i0, i1, i2, i2, i1, i3});
        }
    }
    if (!hemisphere)
        return {std::move(vertices), std::move(indices)};

    for (int longitude = 0; longitude < 4 * quality; ++longitude) {
        std::uint32_t i0 = quality * (4 * quality + 1) + (longitude + 0);
        std::uint32_t i1 = quality * (4 * quality + 1) + (longitude + 1);

        indices.insert(indices.end(), {i0, i1, centre_index});
    }
    return {std::move(vertices), std::move(indices)};
}


std::string readFile(const std::string &file_name, bool verbose = false) {
    // Loads shader from file
    std::string content;
    if (verbose)
        std::cout << "Loading " << file_name << std::endl;

    std::ifstream shader_file(file_name, std::ios::in);
    if (!shader_file.is_open()) {
        throw std::runtime_error("Shader load error: " + file_name);
    }

    std::string line;
    while (!shader_file.eof()) {
        std::getline(shader_file, line);
        content.append(line + "\n");
    }

    shader_file.close();
    return content;
}

GLuint create_shader(GLenum type, const char *source) {
    GLuint result = glCreateShader(type);
    glShaderSource(result, 1, &source, nullptr);
    glCompileShader(result);
    GLint status;
    glGetShaderiv(result, GL_COMPILE_STATUS, &status);
    if (status != GL_TRUE) {
        GLint info_log_length;
        glGetShaderiv(result, GL_INFO_LOG_LENGTH, &info_log_length);
        std::string info_log(info_log_length, '\0');
        glGetShaderInfoLog(result, info_log.size(), nullptr, info_log.data());
        throw std::runtime_error("Shader compilation failed: " + info_log);
    }
    return result;
}

GLuint create_program(GLuint vertex_shader, GLuint fragment_shader) {
    GLuint result = glCreateProgram();
    glAttachShader(result, vertex_shader);
    glAttachShader(result, fragment_shader);
    glLinkProgram(result);

    GLint status;
    glGetProgramiv(result, GL_LINK_STATUS, &status);
    if (status != GL_TRUE) {
        GLint info_log_length;
        glGetProgramiv(result, GL_INFO_LOG_LENGTH, &info_log_length);
        std::string info_log(info_log_length, '\0');
        glGetProgramInfoLog(result, info_log.size(), nullptr, info_log.data());
        throw std::runtime_error("Program linkage failed: " + info_log);
    }

    return result;
}

struct vec2 {
    float x;
    float y;
};

struct mesh {
    GLuint vao;
    gltf_model::accessor indices;
    gltf_model::material material;
};

std::map<std::string, GLint> getLocations(GLuint program, const std::vector<std::string> &loc_names) {
    std::map<std::string, GLint> locations;
    for (auto const &loc_name: loc_names) {
        locations[loc_name] = glGetUniformLocation(program, loc_name.c_str());
    }
    return locations;
}

float clamp(float value, float from = 0, float to = 1) {
    assert(from <= to);
    return fmax(from, fmin(to, value));
}

void setup_attribute(int index, gltf_model::accessor const &accessor, bool integer = false) {
    glEnableVertexAttribArray(index);
    if (integer)
        glVertexAttribIPointer(index, accessor.size, accessor.type, 0,
                               reinterpret_cast<void *>(accessor.view.offset));
    else
        glVertexAttribPointer(index, accessor.size, accessor.type, GL_FALSE, 0,
                              reinterpret_cast<void *>(accessor.view.offset));
};

GLuint create_program(std::string directory, std::string name) {
    std::string vertex_shader_source = readFile(directory + name + ".vert");
    std::string fragment_shader_source = readFile(directory + name + ".frag");
    auto vertex_shader = create_shader(GL_VERTEX_SHADER, vertex_shader_source.data());
    auto fragment_shader = create_shader(GL_FRAGMENT_SHADER, fragment_shader_source.data());
    return create_program(vertex_shader, fragment_shader);
}

GLuint load_texture2D(std::string const &path) {
    int width, height, channels;
    auto pixels = stbi_load(path.data(), &width, &height, &channels, 4);

    GLuint result;
    glGenTextures(1, &result);
    glBindTexture(GL_TEXTURE_2D, result);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glGenerateMipmap(GL_TEXTURE_2D);

    stbi_image_free(pixels);

    return result;
}