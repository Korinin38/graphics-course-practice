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

    // papich
    programs["papich"] = create_program(project_root + "/shaders/", "papich");
    auto papich_locations = getLocations(programs["papich"], {"model",
                                                                      "view",
                                                                      "projection",
                                                                      "light_direction",
                                                                      "albedo",
                                                                      "brightness"});


    const std::string papich_path = project_root + "/external/papich/papich.obj";
    obj_parser::obj_data papich_model = obj_parser::parse_obj(papich_path);
    GLuint papich_vao;
    glGenVertexArrays(1, &papich_vao);
    glBindVertexArray(papich_vao);

    GLuint papich_vbo;
    glGenBuffers(1, &papich_vbo);
    glBindBuffer(GL_ARRAY_BUFFER, papich_vbo);
    glBufferData(GL_ARRAY_BUFFER, papich_model.vertices.size() * sizeof(papich_model.vertices[0]),
                 papich_model.vertices.data(), GL_STATIC_DRAW);

    typedef obj_parser::obj_data::vertex dot_obj_vertex;
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(dot_obj_vertex),
                          (void *) offsetof(dot_obj_vertex, position));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(dot_obj_vertex), (void *) offsetof(dot_obj_vertex, normal));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(dot_obj_vertex),
                          (void *) offsetof(dot_obj_vertex, texcoord));

    GLuint papich_ebo;
    glGenBuffers(1, &papich_ebo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, papich_ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, papich_model.indices.size() * sizeof(papich_model.indices[0]),
                 papich_model.indices.data(), GL_STATIC_DRAW);

    std::map<std::string, GLuint> papich_textures;
    for (auto const &group: papich_model.groups) {

        if (group.material.albedo.empty()) continue;

        papich_textures[group.material.albedo] = load_texture2D(group.material.albedo);
    }

    // Sphere
    programs["sphere"] = create_program(project_root + "/shaders/", "sphere");
    auto sphere_locations = getLocations(programs["sphere"], {{"model",
                                                               "view",
                                                               "projection",
                                                               "light_direction",
                                                               "camera_position",
                                                               "reflection_map",
                                                               "albedo_texture",
                                                               "brightness"}});

    GLuint sphere_vao, sphere_vbo, sphere_ebo;
    glGenVertexArrays(1, &sphere_vao);
    glBindVertexArray(sphere_vao);
    glGenBuffers(1, &sphere_vbo);
    glGenBuffers(1, &sphere_ebo);
    GLuint sphere_index_count;
    {
        auto [vertices, indices] = generate_sphere(1.f, 16);
//        auto [vertices, indices] = generate_sphere(1.f, 16, true);

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

    const int papich_sampler = 2;
    glActiveTexture(GL_TEXTURE0 + papich_sampler);
    GLuint papich_texture = load_texture2D(project_root + "/external/papich/papich.jpg");
    glBindTexture(GL_TEXTURE_2D, papich_texture);

    const int owl_sampler = 3;
    glActiveTexture(GL_TEXTURE0 + owl_sampler);
    GLuint owl_texture = load_texture2D(project_root + "/external/owl.jpg");
    glBindTexture(GL_TEXTURE_2D, owl_texture);

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

        glm::mat4 model(1.f);
        glm::mat4 wolf_model_mat(1.f);
        wolf_model_mat = glm::rotate(wolf_model_mat, -time * 1.13f, {0.f, 1.f, 0.f});
        wolf_model_mat = glm::translate(wolf_model_mat, {0.7f, 0.f, 0.f});
        wolf_model_mat = glm::scale(wolf_model_mat, glm::vec3(0.6f));
        glm::mat4 papich_model_mat(1.f);
        papich_model_mat = glm::translate(papich_model_mat, {0.75f, 0.79f, 0.0f});
        papich_model_mat = glm::rotate(papich_model_mat, -time, {0.f, 1.f, 0.f});
//        papich_model_mat = glm::translate(papich_model_mat, {0.0f, 0.8f, 0.0f});
        papich_model_mat = glm::scale(papich_model_mat, glm::vec3(0.17f, 0.21f, 0.17f));

        glm::mat4 view(1.f);
        view = glm::translate(view, {0.f, 0.f, -camera_distance});
        view = glm::rotate(view, view_angle, {1.f, 0.f, 0.f});
        view = glm::rotate(view, camera_rotation, {0.f, 1.f, 0.f});
        view = glm::translate(view, {0.f, -camera_height, 0.f});

        glm::mat4 projection = glm::perspective(glm::pi<float>() / 2.f, (1.f * width) / height, near, far);

        glm::vec3 camera_position = (glm::inverse(view) * glm::vec4(0.f, 0.f, 0.f, 1.f)).xyz();

//        glm::vec3 light_direction = glm::normalize(glm::vec3(std::cos(time), 1.f, std::sin(time)));
        glm::vec3 light_direction = glm::normalize(glm::vec3(0.f, 1.f, 0.f));


        glm::mat4 view_projection_inverse = glm::inverse(projection * view);


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

        // owl sphere
        glEnable(GL_DEPTH_TEST);
        glDisable(GL_CULL_FACE);

        glUseProgram(programs["sphere"]);
        glUniformMatrix4fv(sphere_locations["model"], 1, GL_FALSE, reinterpret_cast<float *>(&model));
        glUniformMatrix4fv(sphere_locations["view"], 1, GL_FALSE, reinterpret_cast<float *>(&view));
        glUniformMatrix4fv(sphere_locations["projection"], 1, GL_FALSE, reinterpret_cast<float *>(&projection));
        glUniform3fv(sphere_locations["light_direction"], 1, reinterpret_cast<float *>(&light_direction));
        glUniform3fv(sphere_locations["camera_position"], 1, reinterpret_cast<float *>(&camera_position));
        glUniform1i(sphere_locations["reflection_map"], sky_sampler);
        glUniform1i(sphere_locations["albedo_texture"], owl_sampler);
        glUniform1f(sphere_locations["brightness"], brightness);



        glBindVertexArray(sphere_vao);
        glDrawElements(GL_TRIANGLES, sphere_index_count, GL_UNSIGNED_INT, nullptr);

        // papich
        glEnable(GL_DEPTH_TEST);
        glDisable(GL_CULL_FACE);
        glUseProgram(programs["papich"]);
        glUniformMatrix4fv(papich_locations["model"], 1, GL_FALSE, reinterpret_cast<float *>(&papich_model_mat));
        glUniformMatrix4fv(papich_locations["view"], 1, GL_FALSE, reinterpret_cast<float *>(&view));
        glUniformMatrix4fv(papich_locations["projection"], 1, GL_FALSE, reinterpret_cast<float *>(&projection));
        glUniform3fv(papich_locations["light_direction"], 1, reinterpret_cast<float *>(&light_direction));
        glUniform1f(papich_locations["brightness"], brightness);

        glBindVertexArray(papich_vao);
        for (auto const &group: papich_model.groups) {

            if (!group.material.albedo.empty()) {
                glActiveTexture(GL_TEXTURE0 + papich_sampler);
                glBindTexture(GL_TEXTURE_2D, papich_textures[group.material.albedo]);
                glUniform1i(papich_locations["albedo"], papich_sampler);
            }

            glDrawElements(GL_TRIANGLES, group.count, GL_UNSIGNED_INT, reinterpret_cast<void *>(group.offset));
        }

        SDL_GL_SwapWindow(window);
    }

    SDL_GL_DeleteContext(gl_context);
    SDL_DestroyWindow(window);
}
catch (std::exception const &e) {
    std::cerr << e.what() << std::endl;
    return EXIT_FAILURE;
}
