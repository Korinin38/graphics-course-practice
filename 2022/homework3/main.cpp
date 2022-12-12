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

#include "obj_parser.hpp"
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

    // Floor
    programs["floor"] = create_program(project_root + "/shaders/", "floor");

    auto floor_locations = getLocations(programs["floor"], {"model",
                                                            "view",
                                                            "projection",
                                                            "transform",
                                                            "normal_texture",
                                                            "shadow_map",
                                                            "light_direction",
                                                            "brightness"});

    GLuint floor_vao, floor_vbo, floor_ebo;
    glGenVertexArrays(1, &floor_vao);
    glBindVertexArray(floor_vao);
    glGenBuffers(1, &floor_vbo);
    glGenBuffers(1, &floor_ebo);
    GLuint floor_index_count;
    {
        auto [vertices, indices] = generate_sphere(1.f - 0.01f, 16, true);

        glBindBuffer(GL_ARRAY_BUFFER, floor_vbo);
        glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(vertices[0]), vertices.data(), GL_STATIC_DRAW);

        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(vertex), (void *) offsetof(vertex, position));
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(vertex), (void *) offsetof(vertex, tangent));
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(vertex), (void *) offsetof(vertex, normal));
        glEnableVertexAttribArray(3);
        glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, sizeof(vertex), (void *) offsetof(vertex, texcoord));

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, floor_ebo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(indices[0]), indices.data(), GL_STATIC_DRAW);

        floor_index_count = indices.size();
    }
    GLuint floor_normal = load_texture2D(project_root + "/external/snow_normal.png");

    // Lighthouse
    programs["lighthouse"] = create_program(project_root + "/shaders/", "lighthouse");
    auto lighthouse_locations = getLocations(programs["lighthouse"], {"model",
                                                                      "view",
                                                                      "projection",
                                                                      "ambient",
                                                                      "light_direction",
                                                                      "transform",
                                                                      "albedo",
                                                                      "shadow_map",
                                                                      "bias"});

    const std::string lighthouse_path =
            project_root + "/external/Octagonal_Lighthouse_v1/_17498_Octagonal_Lighthouse_v1_NEW.obj";
    obj_parser::obj_data lighthouse_model = obj_parser::parse_obj(lighthouse_path);
    GLuint lighthouse_vao;
    glGenVertexArrays(1, &lighthouse_vao);
    glBindVertexArray(lighthouse_vao);

    GLuint lighthouse_vbo;
    glGenBuffers(1, &lighthouse_vbo);
    glBindBuffer(GL_ARRAY_BUFFER, lighthouse_vbo);
    glBufferData(GL_ARRAY_BUFFER, lighthouse_model.vertices.size() * sizeof(lighthouse_model.vertices[0]),
                 lighthouse_model.vertices.data(), GL_STATIC_DRAW);

    typedef obj_parser::obj_data::vertex dot_obj_vertex;
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(dot_obj_vertex),
                          (void *) offsetof(dot_obj_vertex, position));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(dot_obj_vertex), (void *) offsetof(dot_obj_vertex, normal));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(dot_obj_vertex),
                          (void *) offsetof(dot_obj_vertex, texcoord));

    GLuint lighthouse_ebo;
    glGenBuffers(1, &lighthouse_ebo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, lighthouse_ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, lighthouse_model.indices.size() * sizeof(lighthouse_model.indices[0]),
                 lighthouse_model.indices.data(), GL_STATIC_DRAW);

    std::map<std::string, GLuint> lighthouse_textures;
    for (auto const &group: lighthouse_model.groups) {
        if (!group.material.albedo.empty()) continue;

        lighthouse_textures[group.material.albedo] = load_texture2D(group.material.albedo);
    }
    // ...
    // To hell with this. Now wolf is a lighthouse.

    // Shadow
    programs["shadow"] = create_program(project_root + "/shaders/", "shadow");
    auto shadow_locations = getLocations(programs["shadow"], {"model", "transform"});

    GLsizei shadow_map_resolution = 1024;

    GLuint shadow_map;
    glGenTextures(1, &shadow_map);
    glBindTexture(GL_TEXTURE_2D, shadow_map);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RG32F, shadow_map_resolution, shadow_map_resolution, 0, GL_RGBA, GL_FLOAT,
                 nullptr);

    GLuint shadow_fbo;
    glGenFramebuffers(1, &shadow_fbo);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, shadow_fbo);
    glFramebufferTexture(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, shadow_map, 0);

    GLuint shadow_render;
    glGenRenderbuffers(1, &shadow_render);
    glBindRenderbuffer(GL_RENDERBUFFER, shadow_render);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, shadow_map_resolution, shadow_map_resolution);
    glFramebufferRenderbuffer(GL_DRAW_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, shadow_render);

    if (glCheckFramebufferStatus(GL_DRAW_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        throw std::runtime_error("Incomplete framebuffer!");
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);

    // fog
    programs["fog"] = create_program(project_root + "/shaders/", "fog");
    auto fog_locations = getLocations(programs["fog"], {"view",
                                                        "projection",
                                                        "bbox_min",
                                                        "bbox_max",
                                                        "centre",
                                                        "camera_position",
                                                        "light_direction",
                                                        "cloud_texture"});

    GLuint fog_vao, fog_vbo, fog_ebo;
    glGenVertexArrays(1, &fog_vao);
    glBindVertexArray(fog_vao);

    glGenBuffers(1, &fog_vbo);
    glBindBuffer(GL_ARRAY_BUFFER, fog_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(cube_vertices), cube_vertices, GL_STATIC_DRAW);

    glGenBuffers(1, &fog_ebo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, fog_ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(cube_indices), cube_indices, GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, nullptr);

    const std::string cloud_data_path = project_root + "/external/cloud.data";

    GLuint texture;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_3D, texture);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    int x = 128, y = 64, z = 64;
    std::vector<char> pixels(x * y * z);
    std::ifstream input(cloud_data_path, std::ios::binary);
    input.read(pixels.data(), pixels.size());
    glTexImage3D(GL_TEXTURE_3D, 0, GL_R8, x, y, z, 0, GL_RED, GL_UNSIGNED_BYTE, pixels.data());

    const glm::vec3 cloud_bbox_min{-1.f, -1.05f, -1.f};
    const glm::vec3 cloud_bbox_max{1.f, 1.05f, 1.f};
    const glm::vec3 centre{0.f, 0.f, 0.f};

    // Sphere
    programs["sphere"] = create_program(project_root + "/shaders/", "sphere");
    auto sphere_locations = getLocations(programs["sphere"], {{"model",
                                                               "view",
                                                               "projection",
                                                               "light_direction",
                                                               "camera_position",
                                                               "reflection_map",
                                                               "brightness"}});

    GLuint sphere_vao, sphere_vbo, sphere_ebo;
    glGenVertexArrays(1, &sphere_vao);
    glBindVertexArray(sphere_vao);
    glGenBuffers(1, &sphere_vbo);
    glGenBuffers(1, &sphere_ebo);
    GLuint sphere_index_count;
    {
        auto [vertices, indices] = generate_sphere(1.f, 16);

        glBindBuffer(GL_ARRAY_BUFFER, sphere_vbo);
        glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(vertices[0]), vertices.data(), GL_STATIC_DRAW);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, sphere_ebo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(indices[0]), indices.data(), GL_STATIC_DRAW);

        sphere_index_count = indices.size();
    }
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(vertex), (void *)offsetof(vertex, position));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(vertex), (void *)offsetof(vertex, tangent));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(vertex), (void *)offsetof(vertex, normal));
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, sizeof(vertex), (void *)offsetof(vertex, texcoord));

    // Assigning some wolf_textures to fixed texture units
    const int sky_sampler = 1;
    glActiveTexture(GL_TEXTURE0 + sky_sampler);
    glBindTexture(GL_TEXTURE_2D, environment_map);

    const int wolf_sampler = 2;
    const int floor_sampler = 3;
    glActiveTexture(GL_TEXTURE0 + floor_sampler);
    glBindTexture(GL_TEXTURE_2D, floor_normal);
    const int lighthouse_sampler = 4;
    const int shadow_sampler = 5;

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
        wolf_model_mat = glm::rotate(wolf_model_mat, -time * 1.13f, {0.f, 1.f, 0.f});
        wolf_model_mat = glm::translate(wolf_model_mat, {0.7f, 0.f, 0.f});
        wolf_model_mat = glm::scale(wolf_model_mat, glm::vec3(0.6f));
        glm::mat4 lighthouse_model_mat(1.f);
        lighthouse_model_mat = glm::rotate(lighthouse_model_mat, glm::pi<float>() / 2.f, {-1.f, 0.f, 0.f});
        lighthouse_model_mat = glm::translate(lighthouse_model_mat, {0.f, -0.2f, 0.4f});
        lighthouse_model_mat = glm::scale(lighthouse_model_mat, glm::vec3(1.36f));

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

        // lambda for wolf
        auto draw_wolf_meshes = [&](bool transparent) {
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

        // shadow
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, shadow_fbo);
        glClearColor(1.f, 1.f, 0.f, 0.f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glViewport(0, 0, shadow_map_resolution, shadow_map_resolution);

        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LEQUAL);

        glEnable(GL_CULL_FACE);
        glCullFace(GL_BACK);

        glm::vec3 light_z = -light_direction;
        glm::vec3 light_x = glm::normalize(glm::cross(light_z, {0.f, 1.f, 0.f}));
        glm::vec3 light_y = glm::cross(light_x, light_z);

        glm::mat4 transform = glm::mat4{glm::vec4(light_x, 0),
                                        glm::vec4(light_y, 0),
                                        glm::vec4(light_z, 0),
                                        glm::vec4(glm::vec3(0), 1)};
        transform = glm::inverse(transform);

        glUseProgram(programs["shadow"]);
        glUniformMatrix4fv(shadow_locations["model"], 1, GL_FALSE, reinterpret_cast<float *>(&lighthouse_model_mat));
        glUniformMatrix4fv(shadow_locations["transform"], 1, GL_FALSE, reinterpret_cast<float *>(&transform));

        draw_wolf_meshes(false);
        glDepthMask(GL_FALSE);
        draw_wolf_meshes(true);
        glDepthMask(GL_TRUE);

        glActiveTexture(GL_TEXTURE0 + shadow_sampler);
        glBindTexture(GL_TEXTURE_2D, shadow_map);
        glGenerateMipmap(GL_TEXTURE_2D);

        // back to screen framebuffer

        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
        glViewport(0, 0, width, height);

        // skybox

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

        // wolf
        glEnable(GL_DEPTH_TEST);
        glEnable(GL_CULL_FACE);

        glUseProgram(programs["wolf"]);
        glUniformMatrix4fv(wolf_locations["model"], 1, GL_FALSE, reinterpret_cast<float *>(&wolf_model_mat));
        glUniformMatrix4fv(wolf_locations["view"], 1, GL_FALSE, reinterpret_cast<float *>(&view));
        glUniformMatrix4fv(wolf_locations["projection"], 1, GL_FALSE, reinterpret_cast<float *>(&projection));
        glUniform3fv(wolf_locations["light_direction"], 1, reinterpret_cast<float *>(&light_direction));
        glUniformMatrix4x3fv(wolf_locations["bones"], bones.size(), GL_FALSE, reinterpret_cast<float *>(bones.data()));
        glUniform1f(wolf_locations["brightness"], brightness);

        draw_wolf_meshes(false);
        glDepthMask(GL_FALSE);
        draw_wolf_meshes(true);
        glDepthMask(GL_TRUE);

        // lighthouse?
        glEnable(GL_DEPTH_TEST);
        glEnable(GL_CULL_FACE);

        glUseProgram(programs["wolf"]);
        glUniformMatrix4fv(wolf_locations["model"], 1, GL_FALSE, reinterpret_cast<float *>(&lighthouse_model_mat));
        std::vector<glm::mat4x3> bones_ = std::vector<glm::mat4x3>(wolf_model.bones.size(), glm::mat4x3(1.f));
        glUniformMatrix4x3fv(wolf_locations["bones"], bones_.size(), GL_FALSE,
                             reinterpret_cast<float *>(bones_.data()));

        draw_wolf_meshes(false);
        glDepthMask(GL_FALSE);
        draw_wolf_meshes(true);
        glDepthMask(GL_TRUE);

//        glUseProgram(programs["lighthouse"]);
//        glUniformMatrix4fv(lighthouse_locations["model"], 1, GL_FALSE, reinterpret_cast<float *>(&lighthouse_model_mat));
//        glUniformMatrix4fv(lighthouse_locations["view"], 1, GL_FALSE, reinterpret_cast<float *>(&view));
//        glUniformMatrix4fv(lighthouse_locations["projection"], 1, GL_FALSE, reinterpret_cast<float *>(&projection));
//        glUniform3fv(lighthouse_locations["light_direction"], 1, reinterpret_cast<float *>(&light_direction));
//        glUniform1f(lighthouse_locations["brightness"], brightness);
//
//        glBindVertexArray(lighthouse_vao);
//        for (auto const &group: lighthouse_model.groups) {
//
//            if (!group.material.albedo.empty()) {
//                glActiveTexture(GL_TEXTURE0 + lighthouse_sampler);
//                glBindTexture(GL_TEXTURE_2D, lighthouse_textures[group.material.albedo]);
//                glUniform1i(lighthouse_locations["albedo"], lighthouse_sampler);
//            }
//
//            glDrawElements(GL_TRIANGLES, group.count, GL_UNSIGNED_INT, reinterpret_cast<void *>(group.offset));
//        }
        // fog
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_CULL_FACE);
        glUseProgram(programs["fog"]);
        glUniformMatrix4fv(fog_locations["view"], 1, GL_FALSE, reinterpret_cast<float *>(&view));
        glUniformMatrix4fv(fog_locations["projection"], 1, GL_FALSE, reinterpret_cast<float *>(&projection));
        glUniform3fv(fog_locations["bbox_min"], 1, reinterpret_cast<const float *>(&cloud_bbox_min));
        glUniform3fv(fog_locations["bbox_max"], 1, reinterpret_cast<const float *>(&cloud_bbox_max));
        glUniform3fv(fog_locations["centre"], 1, reinterpret_cast<const float *>(&centre));
        glUniform3fv(fog_locations["camera_position"], 1, reinterpret_cast<float *>(&camera_position));
        glUniform3fv(fog_locations["light_direction"], 1, reinterpret_cast<float *>(&light_direction));
        glUniform1i(fog_locations["cloud_texture"], 0);

        glBindVertexArray(fog_vao);
        glDrawElements(GL_TRIANGLES, std::size(cube_indices), GL_UNSIGNED_INT, nullptr);

        // floor
        glEnable(GL_DEPTH_TEST);
        glDisable(GL_CULL_FACE);
        glEnable(GL_CULL_FACE);

        glUseProgram(programs["floor"]);
        glUniformMatrix4fv(floor_locations["model"], 1, GL_FALSE, reinterpret_cast<float *>(&model));
        glUniformMatrix4fv(floor_locations["view"], 1, GL_FALSE, reinterpret_cast<float *>(&view));
        glUniformMatrix4fv(floor_locations["projection"], 1, GL_FALSE, reinterpret_cast<float *>(&projection));
        glUniformMatrix4fv(floor_locations["transform"], 1, GL_FALSE, reinterpret_cast<float *>(&transform));
        glUniform3fv(floor_locations["light_direction"], 1, reinterpret_cast<float *>(&light_direction));
        glUniform1i(floor_locations["normal_texture"], floor_sampler);
        glUniform1i(floor_locations["shadow_map"], shadow_sampler);
        glUniform1i(floor_locations["reflection_map"], sky_sampler);
        glUniform1f(floor_locations["brightness"], brightness);

        glBindVertexArray(floor_vao);
        glDrawElements(GL_TRIANGLES, floor_index_count, GL_UNSIGNED_INT, nullptr);

        // sphere
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_CULL_FACE);

        glUseProgram(programs["sphere"]);
        glUniformMatrix4fv(sphere_locations["model"], 1, GL_FALSE, reinterpret_cast<float *>(&model));
        glUniformMatrix4fv(sphere_locations["view"], 1, GL_FALSE, reinterpret_cast<float *>(&view));
        glUniformMatrix4fv(sphere_locations["projection"], 1, GL_FALSE, reinterpret_cast<float *>(&projection));
        glUniform3fv(sphere_locations["light_direction"], 1, reinterpret_cast<float *>(&light_direction));
        glUniform3fv(sphere_locations["camera_position"], 1, reinterpret_cast<float *>(&camera_position));
        glUniform1i(sphere_locations["reflection_map"], sky_sampler);
        glUniform1f(sphere_locations["brightness"], brightness);

        glBindVertexArray(sphere_vao);
        glDrawElements(GL_TRIANGLES, sphere_index_count, GL_UNSIGNED_INT, nullptr);

        SDL_GL_SwapWindow(window);
    }

    SDL_GL_DeleteContext(gl_context);
    SDL_DestroyWindow(window);
}
catch (std::exception const &e) {
    std::cerr << e.what() << std::endl;
    return EXIT_FAILURE;
}
