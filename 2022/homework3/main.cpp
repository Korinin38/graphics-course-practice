#ifdef WIN32
#include <SDL.h>
#undef main
#else

#include <SDL2/SDL.h>

#endif

#include <GL/glew.h>

#include <string_view>
#include <stdexcept>
#include <iostream>
#include <fstream>
#include <chrono>
#include <vector>
#include <random>
#include <map>
#include <cmath>

#define GLM_FORCE_SWIZZLE
#define GLM_ENABLE_EXPERIMENTAL

#include <glm/vec3.hpp>
#include <glm/mat4x4.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/scalar_constants.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/string_cast.hpp>

#include "gltf_loader.hpp"
#include "stb_image.h"
#include "main.h"

std::string to_string(std::string_view str) {
    return std::string(str.begin(), str.end());
}

void sdl2_fail(std::string_view message) {
    throw std::runtime_error(to_string(message) + SDL_GetError());
}

void glew_fail(std::string_view message, GLenum error) {
    throw std::runtime_error(to_string(message) + reinterpret_cast<const char *>(glewGetErrorString(error)));
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

struct vertex {
    glm::vec3 position;
    glm::vec3 tangent;
    glm::vec3 normal;
    glm::vec2 texcoord;
};

std::pair<std::vector<vertex>, std::vector<std::uint32_t>> generate_sphere(float radius, int quality) {
    std::vector<vertex> vertices;

    for (int latitude = -quality; latitude <= quality; ++latitude) {
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

    std::vector<std::uint32_t> indices;

    for (int latitude = 0; latitude < 2 * quality; ++latitude) {
        for (int longitude = 0; longitude < 4 * quality; ++longitude) {
            std::uint32_t i0 = (latitude + 0) * (4 * quality + 1) + (longitude + 0);
            std::uint32_t i1 = (latitude + 1) * (4 * quality + 1) + (longitude + 0);
            std::uint32_t i2 = (latitude + 0) * (4 * quality + 1) + (longitude + 1);
            std::uint32_t i3 = (latitude + 1) * (4 * quality + 1) + (longitude + 1);

            indices.insert(indices.end(), {i0, i1, i2, i2, i1, i3});
        }
    }

    return {std::move(vertices), std::move(indices)};
}


int main() try {
    if (SDL_Init(SDL_INIT_VIDEO) != 0)
        sdl2_fail("SDL_Init: ");

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1);
    SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 16);
    SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);

    SDL_Window *window = SDL_CreateWindow("Graphics course practice 11",
                                          SDL_WINDOWPOS_CENTERED,
                                          SDL_WINDOWPOS_CENTERED,
                                          800, 600,
                                          SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_MAXIMIZED);

    if (!window)
        sdl2_fail("SDL_CreateWindow: ");

    int width, height;
    SDL_GetWindowSize(window, &width, &height);

    SDL_GLContext gl_context = SDL_GL_CreateContext(window);
    if (!gl_context)
        sdl2_fail("SDL_GL_CreateContext: ");

    if (auto result = glewInit(); result != GLEW_NO_ERROR)
        glew_fail("glewInit: ", result);

    if (!GLEW_VERSION_3_3)
        throw std::runtime_error("OpenGL 3.3 is not supported");

    const std::string project_root = PROJECT_ROOT;

    // Environment
    std::string sky_vertex_shader_source = readFile(project_root +"/shaders/environment.vert");
    std::string sky_fragment_shader_source = readFile(project_root +"/shaders/environment.frag");
    auto sky_vertex_shader = create_shader(GL_VERTEX_SHADER, sky_vertex_shader_source.data());
    auto sky_fragment_shader = create_shader(GL_FRAGMENT_SHADER, sky_fragment_shader_source.data());
    auto sky_program = create_program(sky_vertex_shader, sky_fragment_shader);
    auto sky_locations = getLocations(sky_program, {"view_projection_inverse",
                                                    "environment_map",
                                                    "camera_position",
                                                    "brightness"});
    GLuint skybox_vao;
    glGenVertexArrays(1, &skybox_vao);
    GLuint environment_map = load_texture2D(project_root + "/external/environment_map.jpg");


    // ?
//    std::string vertex_shader_source = readFile("shaders/debug.vert");
//    std::string fragment_shader_source = readFile("shaders/debug.frag");
//    auto vertex_shader = create_shader(GL_VERTEX_SHADER, vertex_shader_source.c_str());
//    auto fragment_shader = create_shader(GL_FRAGMENT_SHADER, fragment_shader_source.c_str());
//    auto program = create_program(vertex_shader, fragment_shader);
//
//    GLuint model_location = glGetUniformLocation(program, "model");
//    GLuint view_location = glGetUniformLocation(program, "view");
//    GLuint projection_location = glGetUniformLocation(program, "projection");
//    GLuint albedo_location = glGetUniformLocation(program, "albedo");
//    GLuint color_location = glGetUniformLocation(program, "color");
//    GLuint use_texture_location = glGetUniformLocation(program, "use_texture");
//    GLuint light_direction_location = glGetUniformLocation(program, "light_direction");
//    GLuint bones_location = glGetUniformLocation(program, "bones");
//
//    const std::string model_path = project_root + "/wolf/Wolf-Blender-2.82a.gltf";
//
//    auto const input_model = load_gltf(model_path);
//    GLuint vbo;
//    glGenBuffers(1, &vbo);
//    glBindBuffer(GL_ARRAY_BUFFER, vbo);
//    glBufferData(GL_ARRAY_BUFFER, input_model.buffer.size(), input_model.buffer.data(), GL_STATIC_DRAW);
//
//    struct mesh {
//        GLuint vao;
//        gltf_model::accessor indices;
//        gltf_model::material material;
//    };
//
//    auto setup_attribute = [](int index, gltf_model::accessor const &accessor, bool integer = false) {
//        glEnableVertexAttribArray(index);
//        if (integer)
//            glVertexAttribIPointer(index, accessor.size, accessor.type, 0,
//                                   reinterpret_cast<void *>(accessor.view.offset));
//        else
//            glVertexAttribPointer(index, accessor.size, accessor.type, GL_FALSE, 0,
//                                  reinterpret_cast<void *>(accessor.view.offset));
//    };
//
//    std::vector<mesh> meshes;
//    for (auto const &mesh: input_model.meshes) {
//        auto &result = meshes.emplace_back();
//        glGenVertexArrays(1, &result.vao);
//        glBindVertexArray(result.vao);
//
//        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vbo);
//        result.indices = mesh.indices;
//
//        setup_attribute(0, mesh.position);
//        setup_attribute(1, mesh.normal);
//        setup_attribute(2, mesh.texcoord);
//        setup_attribute(3, mesh.joints, true);
//        setup_attribute(4, mesh.weights);
//
//        result.material = mesh.material;
//    }
//
//    std::map<std::string, GLuint> textures;
//    for (auto const &mesh: meshes) {
//        if (!mesh.material.texture_path) continue;
//        if (textures.contains(*mesh.material.texture_path)) continue;
//
//        auto path = std::filesystem::path(model_path).parent_path() / *mesh.material.texture_path;
//
//        int texture_width, texture_height, channels;
//        auto data = stbi_load(path.c_str(), &texture_width, &texture_height, &channels, 4);
//        assert(data);
//
//        GLuint texture;
//        glGenTextures(1, &texture);
//        glBindTexture(GL_TEXTURE_2D, texture);
//        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
//        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
//        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, texture_width, texture_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
//        glGenerateMipmap(GL_TEXTURE_2D);
//
//        stbi_image_free(data);
//
//        textures[*mesh.material.texture_path] = texture;
//    }

    // Assigning some textures to fixed texture units
    const int sky_sampler = 1;
    glActiveTexture(GL_TEXTURE0 + sky_sampler);
    glBindTexture(GL_TEXTURE_2D, environment_map);

    // In-loop variables
    auto last_frame_start = std::chrono::high_resolution_clock::now();

    float time = 0.f;
    float brightness = 0.8f;
    const float brightness_speed = 0.7f;

    std::map<SDL_Keycode, bool> button_down;

    float view_angle = glm::pi<float>() / 8.f;
    float camera_distance = 0.75f;

    float camera_rotation = glm::pi<float>() * (-1.f / 3.f);
    float camera_height = 0.25f;
    const float animation_speed = 1.f;

    bool paused = false;
    float interpolation = 0.f;

    bool running = true;
    while (running) {
        for (SDL_Event event; SDL_PollEvent(&event);)
            switch (event.type) {
                case SDL_QUIT:
                    running = false;
                    break;
                case SDL_WINDOWEVENT:
                    switch (event.window.event) {
                        case SDL_WINDOWEVENT_RESIZED:
                            width = event.window.data1;
                            height = event.window.data2;
                            glViewport(0, 0, width, height);
                            break;
                    }
                    break;
                case SDL_KEYDOWN:
                    button_down[event.key.keysym.sym] = true;
                    if (event.key.keysym.sym == SDLK_SPACE)
                        paused = !paused;
                    break;
                case SDL_KEYUP:
                    button_down[event.key.keysym.sym] = false;
                    break;
            }

        if (!running)
            break;

        auto now = std::chrono::high_resolution_clock::now();
        float dt = std::chrono::duration_cast<std::chrono::duration<float>>(now - last_frame_start).count();
        last_frame_start = now;

        if (!paused)
            time += dt;

        if (button_down[SDLK_UP])
            camera_distance -= 3.f * dt;
        if (button_down[SDLK_DOWN])
            camera_distance += 3.f * dt;

        if (button_down[SDLK_a])
            camera_rotation -= 2.f * dt;
        if (button_down[SDLK_d])
            camera_rotation += 2.f * dt;

        if (button_down[SDLK_w])
            view_angle -= 2.f * dt;
        if (button_down[SDLK_s])
            view_angle += 2.f * dt;
        if (button_down[SDLK_LSHIFT])
            interpolation = clamp(interpolation + animation_speed * dt);
        else
            interpolation = clamp(interpolation - animation_speed * dt);
        if (button_down[SDLK_MINUS])
            brightness = clamp(brightness - brightness_speed * dt, 0.1f, 1.f);
        if (button_down[SDLK_EQUALS])
            brightness = clamp(brightness + brightness_speed * dt, 0.1f, 1.f);



        glClearColor(0.8f, 0.8f, 1.f, 0.f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glEnable(GL_DEPTH_TEST);

        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        float near = 0.01f;
        float far = 100.f;
        float scale = 0.75f + cos(time) * 0.25f;

        glm::mat4 model(1.f);

        glm::mat4 view(1.f);
        view = glm::translate(view, {0.f, 0.f, -camera_distance});
        view = glm::rotate(view, view_angle, {1.f, 0.f, 0.f});
        view = glm::rotate(view, camera_rotation, {0.f, 1.f, 0.f});
        view = glm::translate(view, {0.f, -camera_height, 0.f});

        glm::mat4 projection = glm::perspective(glm::pi<float>() / 2.f, (1.f * width) / height, near, far);

        glm::vec3 camera_position = (glm::inverse(view) * glm::vec4(0.f, 0.f, 0.f, 1.f)).xyz();

        glm::vec3 light_direction = glm::normalize(glm::vec3(1.f, 2.f, 3.f));

//        std::vector<glm::mat4x3> bones = std::vector<glm::mat4x3>(input_model.bones.size(), glm::mat4x3(scale));

        glm::mat4 view_projection_inverse = glm::inverse( projection * view);

        glDisable(GL_DEPTH_TEST);
        glDisable(GL_CULL_FACE);

        glUseProgram(sky_program);
        glUniform3fv(sky_locations["camera_position"], 1, reinterpret_cast<float *>(&camera_position));
        glUniformMatrix4fv(sky_locations["view_projection_inverse"], 1, GL_FALSE, reinterpret_cast<float *>(&view_projection_inverse));
        glUniform1i(sky_locations["environment_map"], sky_sampler);
        glUniform1f(sky_locations["brightness"], brightness);

        glBindVertexArray(skybox_vao);
        glDrawArrays(GL_TRIANGLES, 0, 6);


//        glUseProgram(program);
//        glUniformMatrix4fv(model_location, 1, GL_FALSE, reinterpret_cast<float *>(&model));
//        glUniformMatrix4fv(view_location, 1, GL_FALSE, reinterpret_cast<float *>(&view));
//        glUniformMatrix4fv(projection_location, 1, GL_FALSE, reinterpret_cast<float *>(&projection));
//        glUniform3fv(light_direction_location, 1, reinterpret_cast<float *>(&light_direction));
//        glUniformMatrix4x3fv(bones_location, bones.size(), GL_FALSE, reinterpret_cast<float *>(bones.data()));
//
//        auto draw_meshes = [&](bool transparent) {
//            for (auto const &mesh: meshes) {
//                if (mesh.material.transparent != transparent)
//                    continue;
//
//                if (mesh.material.two_sided)
//                    glDisable(GL_CULL_FACE);
//                else
//                    glEnable(GL_CULL_FACE);
//
//                if (transparent)
//                    glEnable(GL_BLEND);
//                else
//                    glDisable(GL_BLEND);
//
//                if (mesh.material.texture_path) {
//                    glBindTexture(GL_TEXTURE_2D, textures[*mesh.material.texture_path]);
//                    glUniform1i(use_texture_location, 1);
//                } else if (mesh.material.color) {
//                    glUniform1i(use_texture_location, 0);
//                    glUniform4fv(color_location, 1, reinterpret_cast<const float *>(&(*mesh.material.color)));
//                } else
//                    continue;
//
//                glBindVertexArray(mesh.vao);
//                glDrawElements(GL_TRIANGLES, mesh.indices.count, mesh.indices.type,
//                               reinterpret_cast<void *>(mesh.indices.view.offset));
//            }
//        };
//
//        draw_meshes(false);
//        glDepthMask(GL_FALSE);
//        draw_meshes(true);
//        glDepthMask(GL_TRUE);

        SDL_GL_SwapWindow(window);
    }

    SDL_GL_DeleteContext(gl_context);
    SDL_DestroyWindow(window);
}
catch (std::exception const &e) {
    std::cerr << e.what() << std::endl;
    return EXIT_FAILURE;
}
