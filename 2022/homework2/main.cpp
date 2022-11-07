#include "main.h"

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
#include <map>

#include <glm/vec3.hpp>
#include <glm/mat4x4.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/scalar_constants.hpp>
#include <glm/gtx/string_cast.hpp>

#include "obj_parser.hpp"
#include "stb_image.h"


std::string to_string(std::string_view str) {
    return {str.begin(), str.end()};
}

void sdl2_fail(std::string_view message) {
    throw std::runtime_error(to_string(message) + SDL_GetError());
}

void glew_fail(std::string_view message, GLenum error) {
    throw std::runtime_error(to_string(message) + reinterpret_cast<const char *>(glewGetErrorString(error)));
}

int main(int argc, char *argv[]) try {
    if (argc < 2) {
        throw std::invalid_argument("Expected \"*.obj\" file path");
    }
    if (argc > 2) {
        std::cout << "Warning: expected 1 argument, got " << argc - 1 << " instead." << std::endl;
    }

    if (SDL_Init(SDL_INIT_VIDEO) != 0)
        sdl2_fail("SDL_Init: ");

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1);
    SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 4);

    SDL_Window *window = SDL_CreateWindow("Graphics course homework 2",
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

    SDL_GL_SetSwapInterval(0);

    if (auto result = glewInit(); result != GLEW_NO_ERROR)
        glew_fail("glewInit: ", result);

    if (!GLEW_VERSION_3_3)
        throw std::runtime_error("OpenGL 3.3 is not supported");

    glClearColor(0.8f, 0.8f, 1.f, 0.f);

    glEnable(GL_PRIMITIVE_RESTART);
    glPrimitiveRestartIndex(config.PRIMITIVE_RESTART_INDEX);

    std::string scene_v_shader_source = readFile("shaders/scene.vert");
    std::string scene_frag_shader_source = readFile("shaders/scene.frag");

    std::string global_shadow_v_shader_source = readFile("shaders/global_shadow.vert");
    std::string global_frag_shader_source = readFile("shaders/global_shadow.frag");

    auto program = create_program(
            create_shader(GL_VERTEX_SHADER, scene_v_shader_source.data()),
            create_shader(GL_FRAGMENT_SHADER, scene_frag_shader_source.data()));

    auto global_shadow_program = create_program(
            create_shader(GL_VERTEX_SHADER, global_shadow_v_shader_source.data()),
            create_shader(GL_FRAGMENT_SHADER, global_frag_shader_source.data()));

    // debug
    std::string debug_vertex_shader_source = readFile("shaders/debug.vert");
    std::string debug_fragment_shader_source = readFile("shaders/debug.frag");
    auto debug_vertex_shader = create_shader(GL_VERTEX_SHADER, debug_vertex_shader_source.data());
    auto debug_fragment_shader = create_shader(GL_FRAGMENT_SHADER, debug_fragment_shader_source.data());
    auto debug_program = create_program(debug_vertex_shader, debug_fragment_shader);

    GLuint debug_shadow_map_location = glGetUniformLocation(debug_program, "shadow_map");
    glUseProgram(debug_program);
    glUniform1i(debug_shadow_map_location, 0);

    GLuint debug_vao;
    glGenVertexArrays(1, &debug_vao);
    // !debug

    auto shadow_model_location = glGetUniformLocation(global_shadow_program, "model");
    auto shadow_transform_location = glGetUniformLocation(global_shadow_program, "transform");

    GLsizei shadow_map_resolution = 1024;

    GLuint shadow_map;
    glGenTextures(1, &shadow_map);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, shadow_map);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RG32F, shadow_map_resolution, shadow_map_resolution, 0, GL_RGBA, GL_FLOAT, nullptr);

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

    std::string scene_path = argv[1];
    obj_parser::obj_data scene = obj_parser::parse_obj(scene_path);

    // Bounding box
    // 0 - minimum, 1 - maximum
    float X[2] = {scene.vertices[0].position[0], scene.vertices[0].position[0]};
    float Y[2] = {scene.vertices[0].position[1], scene.vertices[0].position[1]};
    float Z[2] = {scene.vertices[0].position[2], scene.vertices[0].position[2]};
    for (auto v : scene.vertices) {
        if (v.position[0] < X[0]) {
            X[0] = v.position[0];
        }
        if (v.position[0] > X[1]) {
            X[1] = v.position[0];
        }
        if (v.position[1] < Y[0]) {
            Y[0] = v.position[1];
        }
        if (v.position[1] > Y[1]) {
            Y[1] = v.position[1];
        }
        if (v.position[2] < Z[0]) {
            Z[0] = v.position[2];
        }
        if (v.position[2] > Z[1]) {
            Z[1] = v.position[2];
        }
    }
    glm::vec3 center((X[1] + X[0]) / 2, (Y[1] + Y[0]) / 2, (Z[1] + Z[0]) / 2);
//
//    std::cout << X[0] << " " << center[0] << " " << X[1] << std::endl;
//    std::cout << Y[0] << " " << center[1] << " " << Y[1] << std::endl;
//    std::cout << Z[0] << " " << center[2] << " " << Z[1] << std::endl;

    // Load textures
    GLuint texture;
//    glGenTextures(1, &texture);
//    glActiveTexture(GL_TEXTURE0);
//    glBindTexture(GL_TEXTURE_2D, texture);
//    int a[4] = {0, 0, 0, 0};
//    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, a);

    std::map<std::string, std::pair<GLuint, int>> textures_albedo;
    std::map<std::string, std::pair<GLuint, int>> textures_transparency;
    int tex_num = 0;
    for (auto &group: scene.groups) {
        if (group.material.albedo.empty()) {
//            std::cout << "no texture!" << std::endl;
            continue;
        }
        if (textures_albedo.find(group.material.albedo) != textures_albedo.end())
            continue;

        glGenTextures(1, &texture);
        int unit = tex_num + 1;
        textures_albedo[group.material.albedo] = {texture, unit};
        glActiveTexture(GL_TEXTURE1);
//        glActiveTexture(GL_TEXTURE0 + unit);
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

        int x, y, n;
        unsigned char *texture_data = stbi_load(group.material.albedo.c_str(), &x, &y, &n, 4);
        if (texture_data == nullptr) {
            std::cout << "Cannot load texture " << group.material.albedo.c_str() << ":" << stbi_failure_reason() << std::endl;
            throw std::runtime_error("Cannot load texture");
        }
//        if (n == 4)
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, x, y, 0, GL_RGBA, GL_UNSIGNED_BYTE, texture_data);
//        else if (n == 3)
//            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, x, y, 0, GL_RGBA, GL_UNSIGNED_BYTE, texture_data);
        glGenerateMipmap(GL_TEXTURE_2D);

        stbi_image_free(texture_data);
        ++tex_num;

        if (group.material.transparency.empty()) {
//            std::cout << "no transparency!" << std::endl;
            continue;
        }
        else {
//            std::cout << group.material.transparency << std::endl;
        }
        if (textures_transparency.find(group.material.transparency) != textures_transparency.end())
            continue;
        glGenTextures(1, &texture);
        unit = tex_num + 1;

        textures_transparency[group.material.transparency] = {texture, unit};
        glActiveTexture(GL_TEXTURE2);
//        glActiveTexture(GL_TEXTURE0 + unit);
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

        texture_data = stbi_load(group.material.transparency.c_str(), &x, &y, &n, 4);
        if (texture_data == nullptr) {
            std::cout << "Cannot load transparency " << group.material.transparency.c_str() << ":" << stbi_failure_reason() << std::endl;
            throw std::runtime_error("Cannot load transparency");
        }
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, x, y, 0, GL_RGBA, GL_UNSIGNED_BYTE, texture_data);
        glGenerateMipmap(GL_TEXTURE_2D);
        stbi_image_free(texture_data);
        ++tex_num;
    }

    // Uniforms - vertex
    auto model_location = glGetUniformLocation(program, "model");
    auto view_location = glGetUniformLocation(program, "view");
    auto projection_location = glGetUniformLocation(program, "projection");

    // Uniforms - fragment
    auto camera_position_location = glGetUniformLocation(program, "camera_position");
    auto sun_direction_location = glGetUniformLocation(program, "sun_direction");
    auto sun_color_location = glGetUniformLocation(program, "sun_color");
    auto glossiness_location = glGetUniformLocation(program, "glossiness");
    auto roughness_location = glGetUniformLocation(program, "roughness");
    auto albedo_location = glGetUniformLocation(program, "albedo");
    auto transparency_location = glGetUniformLocation(program, "transparency");
    auto solid_location = glGetUniformLocation(program, "solid");

    auto global_shadow_map_location = glGetUniformLocation(program, "shadow_map");
    auto global_shadow_transform_location = glGetUniformLocation(program, "transform");
    auto shadow_bias_location = glGetUniformLocation(program, "bias");

    float time = 0.f;

    auto last_frame_start = std::chrono::high_resolution_clock::now();

    GLuint scene_vao, scene_vbo, scene_ebo;
    glGenVertexArrays(1, &scene_vao);
    glBindVertexArray(scene_vao);

    glGenBuffers(1, &scene_vbo);
    glBindBuffer(GL_ARRAY_BUFFER, scene_vbo);
    glBufferData(GL_ARRAY_BUFFER, scene.vertices.size() * sizeof(scene.vertices[0]), scene.vertices.data(),
                 GL_STATIC_DRAW);

    glGenBuffers(1, &scene_ebo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, scene_ebo);

    glBufferData(GL_ELEMENT_ARRAY_BUFFER, scene.indices.size() * sizeof(scene.indices[0]), scene.indices.data(),
                 GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(obj_parser::obj_data::vertex), nullptr);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(obj_parser::obj_data::vertex), (void *) (12));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(obj_parser::obj_data::vertex), (void *) (24));

    std::map<SDL_Keycode, bool> button_down;


    glm::vec3 camera_pos   = glm::vec3(0.0f, 0.0f,  3.0f);
    glm::vec3 camera_front = glm::vec3(0.0f, 0.0f, -1.0f);
    glm::vec3 camera_up    = glm::vec3(0.0f, 1.0f,  0.0f);


    glm::vec3 camera_distance = {0.f, 0.f, 0.f};
    float camera_pitch = 0.f;
    float camera_yaw = glm::pi<float>();
    float camera_roll = 0.f;
    float map_size = std::max(std::max(X[1] - X[0], Y[1] - Y[0]), Z[1] - Z[0]);

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
                            break;
                    }
                    break;
                case SDL_KEYDOWN:
                    button_down[event.key.keysym.sym] = true;
                    break;
                case SDL_KEYUP:
                    button_down[event.key.keysym.sym] = false;
                    break;
            }

        if (!running)
            break;


        if (button_down[SDLK_p])
            continue;

        auto now = std::chrono::high_resolution_clock::now();
        float dt = std::chrono::duration_cast<std::chrono::duration<float>>(now - last_frame_start).count();
        last_frame_start = now;
        time += dt;

        float camera_speed = map_size / 60.0;
        if (button_down[SDLK_LSHIFT])
            camera_speed *= 3;
        if (button_down[SDLK_LCTRL])
            camera_speed *= 3;

        if (button_down[SDLK_w])
            camera_pos += camera_front * camera_speed * dt;
        if (button_down[SDLK_s])
            camera_pos -= camera_front * camera_speed * dt;

        if (button_down[SDLK_a])
            camera_pos -= glm::normalize(glm::cross(camera_front, camera_up)) * camera_speed * dt;
        if (button_down[SDLK_d])
            camera_pos += glm::normalize(glm::cross(camera_front, camera_up)) * camera_speed * dt;

        if (button_down[SDLK_SPACE])
            camera_pos += camera_up * camera_speed * dt;
        if (button_down[SDLK_c])
            camera_pos -= camera_up * camera_speed * dt;

        if (button_down[SDLK_LEFT])
            camera_yaw -= 2.f * dt;
        if (button_down[SDLK_RIGHT])
            camera_yaw += 2.f * dt;

        if (button_down[SDLK_UP])
            camera_pitch += 2.f * dt;
        if (button_down[SDLK_DOWN])
            camera_pitch -= 2.f * dt;

        if (button_down[SDLK_q])
            camera_roll += 2.f * dt;
        if (button_down[SDLK_e])
            camera_roll -= 2.f * dt;


        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, shadow_fbo);
        glClearColor(1.f, 1.f, 0.f, 0.f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glViewport(0, 0, shadow_map_resolution, shadow_map_resolution);

        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LEQUAL);

        glEnable(GL_CULL_FACE);
        glCullFace(GL_BACK);

        glm::mat4 model(1.f);

        glm::vec3 light_direction = glm::normalize(glm::vec3(std::cos(time * 0.5f), 1.f, std::sin(time * 0.5f)));

        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LEQUAL);

        glEnable(GL_CULL_FACE);
        glCullFace(GL_BACK);

        glm::vec3 light_z = -light_direction;
        glm::vec3 light_x = glm::normalize(glm::cross(light_z, {0.f, 1.f, 0.f}));
        glm::vec3 light_y = glm::cross(light_x, light_z);

        glm::vec3 max_len = { 0.f, 0.f, 0.f };
        for (int ix = 0; ix <= 1; ++ix) {
            for (int iy = 0; iy <= 1; ++ iy) {
                for (int iz = 0; iz <= 1; ++iz) {
                    glm::vec3 v = glm::vec3(X[ix], Y[iy], Z[iz]) - center;
                    max_len[0] = fmax(max_len[0], abs(glm::dot(v, light_x)));
                    max_len[1] = fmax(max_len[1], abs(glm::dot(v, light_y)));
                    max_len[2] = fmax(max_len[2], abs(glm::dot(v, light_z)));
                }
            }
        }
        light_x *= max_len[0];
        light_y *= max_len[1];
        light_z *= max_len[2];

        glm::mat4 transform = glm::mat4 { glm::vec4(light_x, 0),
                                          glm::vec4(light_y, 0),
                                          glm::vec4(light_z, 0),
                                          glm::vec4(center, 1) };
        transform = glm::inverse(transform);

        glUseProgram(global_shadow_program);
        glUniformMatrix4fv(shadow_model_location, 1, GL_FALSE, reinterpret_cast<float *>(&model));
        glUniformMatrix4fv(shadow_transform_location, 1, GL_FALSE, reinterpret_cast<float *>(&transform));

        glBindVertexArray(scene_vao);
        glDrawElements(GL_TRIANGLES, scene.indices.size(), GL_UNSIGNED_INT, nullptr);

        camera_pitch = std::max(-glm::pi<float>() / 2 + 0.01f, std::min(glm::pi<float>() / 2 - 0.01f, camera_pitch));

        glm::vec3 direction;
        direction.x = cos(camera_yaw) * cos(camera_pitch); // Note that we convert the angle to radians first
        direction.y = sin(camera_pitch);
        direction.z = sin(camera_yaw) * cos(camera_pitch);
        camera_front = glm::normalize(direction);

        glm::mat4 view(1.f);
        view = glm::lookAt(camera_pos, camera_pos + camera_front, camera_up);

        float near = 0.1f;
        float far = map_size * 1.6f;

        float aspect = (float) height / (float) width;
        glm::mat4 projection = glm::perspective(glm::pi<float>() / 3.f, 1.f / aspect, near, far);

        glm::vec3 camera_position = glm::vec3(glm::inverse(view) * glm::vec4(0.f, 0.f, 0.f, 1.f));

        glm::vec3 sun_direction = glm::normalize(glm::vec3(std::sin(time * 0.5f), 3.f, std::cos(time * 0.5f)));

        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
        glViewport(0, 0, width, height);

        glClearColor(0.8f, 0.8f, 1.f, 0.f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LEQUAL);

        glEnable(GL_CULL_FACE);
        glCullFace(GL_BACK);

        glUseProgram(program);

        glUniformMatrix4fv(model_location, 1, GL_FALSE, reinterpret_cast<float *>(&model));
        glUniformMatrix4fv(view_location, 1, GL_FALSE, reinterpret_cast<float *>(&view));
        glUniformMatrix4fv(projection_location, 1, GL_FALSE, reinterpret_cast<float *>(&projection));
        glUniform3fv(camera_position_location, 1, (float *) (&camera_position));
        glUniform3f(sun_color_location, 1.f, 1.f, 1.f);
        glUniform3fv(sun_direction_location, 1, reinterpret_cast<float *>(&sun_direction));

        glUniform1i(global_shadow_map_location, 0);
        glUniformMatrix4fv(global_shadow_transform_location, 1, GL_FALSE, reinterpret_cast<float *>(&transform));
        glUniform1f(shadow_bias_location, 0.01f);

        glBindVertexArray(scene_vao);
        tex_num = 0;
        for (auto & group: scene.groups) {

//            std::cout << "group of " << group.material.name
//                      << " with texture material " << group.material.albedo
//                      << " and alpha-map " << group.material.transparency << std::endl;
            if  (group.material.albedo.empty()) {
//                glUniform1i(albedo_location, 0);
            }
            else {
                glActiveTexture(GL_TEXTURE1);
                glBindTexture(GL_TEXTURE_2D, textures_albedo[group.material.albedo].first);
                glUniform1i(albedo_location, 1);
//                glUniform1i(albedo_location, textures_albedo[group.material.albedo].second);
            }

            if  (group.material.transparency.empty()) {
                glUniform1i(solid_location, 1);
//                glUniform1i(transparency_location, 1);
            }
            else {
//                if (group.material.name == "Glass_Pane")
//                    std::cout << "aaaaa" << std::endl;
                glUniform1i(solid_location, 0);
                glActiveTexture(GL_TEXTURE2);
                glBindTexture(GL_TEXTURE_2D, textures_transparency[group.material.transparency].first);
                glUniform1i(transparency_location, 2);
//                glUniform1i(transparency_location, textures_transparency[group.material.transparency].second);
            }

            glUniform1i(solid_location, 0);
            glUniform3fv(glossiness_location, 1, reinterpret_cast<float *>(&group.material.glossiness));
            glUniform1f(roughness_location, group.material.roughness);

            glDrawElements(GL_TRIANGLES, group.count, GL_UNSIGNED_INT, (uint32_t*)nullptr + group.offset);
        }

        glUseProgram(debug_program);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, shadow_map);
        glUniform1i(debug_shadow_map_location, 0);

        glBindVertexArray(debug_vao);
        glDrawArrays(GL_TRIANGLES, 0, 6);

        SDL_GL_SwapWindow(window);
    }

    SDL_GL_DeleteContext(gl_context);
    SDL_DestroyWindow(window);
}
catch (std::exception const &e) {
    std::cerr << e.what() << std::endl;
    return EXIT_FAILURE;
}
