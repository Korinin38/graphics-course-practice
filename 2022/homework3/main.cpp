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
    std::map<std::string, GLuint> programs;

    // Environment
    programs["sky"] = create_program(project_root + "/shaders/", "environment");

    auto sky_locations = getLocations(programs["sky"], {"view_projection_inverse",
                                                        "environment_map",
                                                        "camera_position",
                                                        "brightness"});
    GLuint skybox_vao;
    glGenVertexArrays(1, &skybox_vao);
    GLuint environment_map = load_texture2D(project_root + "/external/environment_map.jpg");

    // Wolf
    programs["wolf"] = create_program(project_root + "/shaders/", "wolf");

    auto wolf_locations = getLocations(programs["wolf"], {"model",
                                                          "view",
                                                          "projection",
                                                          "albedo",
                                                          "color",
                                                          "use_texture",
                                                          "light_direction",
                                                          "bones",
                                                          "brightness"});

    const std::string wolf_path = project_root + "/external/wolf/Wolf-Blender-2.82a.gltf";
    auto const wolf_model = load_gltf(wolf_path);

    GLuint wolf_vbo;
    glGenBuffers(1, &wolf_vbo);
    glBindBuffer(GL_ARRAY_BUFFER, wolf_vbo);
    glBufferData(GL_ARRAY_BUFFER, wolf_model.buffer.size(), wolf_model.buffer.data(), GL_STATIC_DRAW);

    std::vector<mesh> wolf_meshes;
    for (auto const &mesh: wolf_model.meshes) {
        auto &result = wolf_meshes.emplace_back();
        glGenVertexArrays(1, &result.vao);
        glBindVertexArray(result.vao);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, wolf_vbo);
        result.indices = mesh.indices;

        setup_attribute(0, mesh.position);
        setup_attribute(1, mesh.normal);
        setup_attribute(2, mesh.texcoord);
        setup_attribute(3, mesh.joints, true);
        setup_attribute(4, mesh.weights);

        result.material = mesh.material;
    }

    std::map<std::string, GLuint> wolf_textures;
    for (auto const &mesh: wolf_meshes) {
        if (!mesh.material.texture_path) continue;
        if (wolf_textures.contains(*mesh.material.texture_path)) continue;

        auto path = std::filesystem::path(wolf_path).parent_path() / *mesh.material.texture_path;

        wolf_textures[*mesh.material.texture_path] = load_texture2D(path);
    }

    // Snow
    programs["snow"] = create_program(project_root + "/shaders/", "snow");

    auto snow_locations = getLocations(programs["snow"], {"model",
                                                              "view",
                                                              "projection",
                                                              "normal_texture",
                                                              "reflection_map",
                                                              "light_direction",
                                                              "camera_position",
                                                              "brightness"});

    GLuint snow_vao, snow_vbo, snow_ebo;
    glGenVertexArrays(1, &snow_vao);
    glBindVertexArray(snow_vao);
    glGenBuffers(1, &snow_vbo);
    glGenBuffers(1, &snow_ebo);
    GLuint snow_index_count;
    {
        auto [vertices, indices] = generate_sphere(1.f, 16, true);

        glBindBuffer(GL_ARRAY_BUFFER, snow_vbo);
        glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(vertices[0]), vertices.data(), GL_STATIC_DRAW);

        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(vertex), (void *)offsetof(vertex, position));
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(vertex), (void *)offsetof(vertex, tangent));
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(vertex), (void *)offsetof(vertex, normal));
        glEnableVertexAttribArray(3);
        glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, sizeof(vertex), (void *)offsetof(vertex, texcoord));

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, snow_ebo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(indices[0]), indices.data(), GL_STATIC_DRAW);

        snow_index_count = indices.size();
    }
    GLuint snow_normal = load_texture2D(project_root + "/external/snow_normal.png");

    // Assigning some wolf_textures to fixed texture units
    const int sky_sampler = 1;
    glActiveTexture(GL_TEXTURE0 + sky_sampler);
    glBindTexture(GL_TEXTURE_2D, environment_map);

    const int wolf_sampler = 2;
    const int snow_sampler = 3;
    glActiveTexture(GL_TEXTURE0 + snow_sampler);
    glBindTexture(GL_TEXTURE_2D, snow_normal);

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
        glm::mat4 wolf_model_mat(1.f);
        wolf_model_mat = glm::rotate (wolf_model_mat, -time, {0.f, 1.f, 0.f});
        wolf_model_mat = glm::translate (wolf_model_mat, {0.7f, 0.f, 0.f});
        wolf_model_mat = glm::scale(wolf_model_mat, glm::vec3(0.6f));

        glm::mat4 view(1.f);
        view = glm::translate(view, {0.f, 0.f, -camera_distance});
        view = glm::rotate(view, view_angle, {1.f, 0.f, 0.f});
        view = glm::rotate(view, camera_rotation, {0.f, 1.f, 0.f});
        view = glm::translate(view, {0.f, -camera_height, 0.f});

        glm::mat4 projection = glm::perspective(glm::pi<float>() / 2.f, (1.f * width) / height, near, far);

        glm::vec3 camera_position = (glm::inverse(view) * glm::vec4(0.f, 0.f, 0.f, 1.f)).xyz();

        glm::vec3 light_direction = glm::normalize(glm::vec3(std::cos(time), 1.f, std::sin(time)));

        std::vector<glm::mat4x3> bones = std::vector<glm::mat4x3>(wolf_model.bones.size(), glm::mat4x3(scale));

        auto run_animation = wolf_model.animations.at("01_Run");
        auto walk_animation = wolf_model.animations.at("02_walk");

        float walk_frame = fmod(time * animation_speed, walk_animation.max_time);
        float run_frame = fmod(time * animation_speed, run_animation.max_time);

        for (int i = 0; i < bones.size(); ++i) {
            glm::mat4 transform = glm::mat4(1.f);
            int p = i;
            while (p != -1) {
                auto walk_bone = walk_animation.bones[p];
                auto run_bone = run_animation.bones[p];

                auto t = walk_bone.translation(walk_frame) * (1 - interpolation) +
                         run_bone.translation(run_frame) * interpolation;
                auto r = walk_bone.rotation(walk_frame) * (1 - interpolation) +
                         run_bone.rotation(run_frame) * interpolation;
                auto s = walk_bone.scale(walk_frame) * (1 - interpolation) + run_bone.scale(run_frame) * interpolation;

                glm::mat4 translation = glm::translate(glm::mat4(1.f), t);
                glm::mat4 rotation = glm::toMat4(r);
                glm::mat4 scaling = glm::scale(glm::mat4(1.f), s);
                transform = translation * rotation * scaling * transform;

                p = wolf_model.bones[p].parent;
            }
            bones[i] = transform;
        }
        for (int i = 0; i < bones.size(); ++i) {
            bones[i] = bones[i] * wolf_model.bones[i].inverse_bind_matrix;
        }

        glm::mat4 view_projection_inverse = glm::inverse(projection * view);


        glDisable(GL_DEPTH_TEST);
        glDisable(GL_CULL_FACE);

        glUseProgram(programs["sky"]);
        glUniform3fv(sky_locations["camera_position"], 1, reinterpret_cast<float *>(&camera_position));
        glUniformMatrix4fv(sky_locations["view_projection_inverse"], 1, GL_FALSE,
                           reinterpret_cast<float *>(&view_projection_inverse));
        glUniform1i(sky_locations["environment_map"], sky_sampler);
        glUniform1f(sky_locations["brightness"], brightness);

        glBindVertexArray(skybox_vao);
        glDrawArrays(GL_TRIANGLES, 0, 6);


        glEnable(GL_DEPTH_TEST);
        glEnable(GL_CULL_FACE);

        glUseProgram(programs["wolf"]);
        glUniformMatrix4fv(wolf_locations["model"], 1, GL_FALSE, reinterpret_cast<float *>(&wolf_model_mat));
        glUniformMatrix4fv(wolf_locations["view"], 1, GL_FALSE, reinterpret_cast<float *>(&view));
        glUniformMatrix4fv(wolf_locations["projection"], 1, GL_FALSE, reinterpret_cast<float *>(&projection));
        glUniform3fv(wolf_locations["light_direction"], 1, reinterpret_cast<float *>(&light_direction));
        glUniformMatrix4x3fv(wolf_locations["bones"], bones.size(), GL_FALSE, reinterpret_cast<float *>(bones.data()));
        glUniform1f(wolf_locations["brightness"], brightness);

        auto draw_meshes = [&](bool transparent) {
            for (auto const &mesh: wolf_meshes) {
                if (mesh.material.transparent != transparent)
                    continue;

                if (mesh.material.two_sided)
                    glDisable(GL_CULL_FACE);
                else
                    glEnable(GL_CULL_FACE);

                if (transparent)
                    glEnable(GL_BLEND);
                else
                    glDisable(GL_BLEND);

                if (mesh.material.texture_path) {
                    glActiveTexture(GL_TEXTURE0 + wolf_sampler);
                    glBindTexture(GL_TEXTURE_2D, wolf_textures[*mesh.material.texture_path]);
                    glUniform1i(wolf_locations["use_texture"], 1);
                    glUniform1i(wolf_locations["albedo"], wolf_sampler);
                } else if (mesh.material.color) {
                    glUniform1i(wolf_locations["use_texture"], 0);
                    glUniform4fv(wolf_locations["color"], 1, reinterpret_cast<const float *>(&(*mesh.material.color)));
                } else
                    continue;

                glBindVertexArray(mesh.vao);
                glDrawElements(GL_TRIANGLES, mesh.indices.count, mesh.indices.type,
                               reinterpret_cast<void *>(mesh.indices.view.offset));
            }
        };

        draw_meshes(false);
        glDepthMask(GL_FALSE);
        draw_meshes(true);
        glDepthMask(GL_TRUE);


        glEnable(GL_DEPTH_TEST);
        glDisable(GL_CULL_FACE);

        glUseProgram(programs["snow"]);
        glUniformMatrix4fv(snow_locations["model"], 1, GL_FALSE, reinterpret_cast<float *>(&model));
        glUniformMatrix4fv(snow_locations["view"], 1, GL_FALSE, reinterpret_cast<float *>(&view));
        glUniformMatrix4fv(snow_locations["projection"], 1, GL_FALSE, reinterpret_cast<float *>(&projection));
        glUniform3fv(snow_locations["light_direction"], 1, reinterpret_cast<float *>(&light_direction));
        glUniform3fv(snow_locations["camera_position"], 1, reinterpret_cast<float *>(&camera_position));
        glUniform1i(snow_locations["normal_texture"], snow_sampler);
        glUniform1i(snow_locations["reflection_map"], sky_sampler);
        glUniform1f(snow_locations["brightness"], brightness);

        glBindVertexArray(snow_vao);
        glDrawElements(GL_TRIANGLES, snow_index_count, GL_UNSIGNED_INT, nullptr);
        glDrawElements(GL_LINES, snow_index_count, GL_UNSIGNED_INT, nullptr);

        SDL_GL_SwapWindow(window);
    }

    SDL_GL_DeleteContext(gl_context);
    SDL_DestroyWindow(window);
}
catch (std::exception const &e) {
    std::cerr << e.what() << std::endl;
    return EXIT_FAILURE;
}
