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

    glClearColor(1.f, 1.f, 1.f, 0.f);

    glEnable(GL_PRIMITIVE_RESTART);
    glPrimitiveRestartIndex(config.PRIMITIVE_RESTART_INDEX);

    std::string grid_v_shader_source = readFile("shaders/scene.vert");
    std::string grid_frag_shader_source = readFile("shaders/scene.frag");

    auto program = create_program(
            create_shader(GL_VERTEX_SHADER, grid_v_shader_source.data()),
            create_shader(GL_FRAGMENT_SHADER, grid_frag_shader_source.data()));

    std::string scene_path = argv[1];
    obj_parser::obj_data scene = obj_parser::parse_obj(scene_path);

    // Load textures
    GLuint texture;
    glGenTextures(1, &texture);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture);
    int a[4] = {0, 0, 0, 0};
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, a);

    std::map<std::string, int> textures_albedo;
    std::map<std::string, int> textures_transparency;
    int tex_num = 0;
    for (auto &[name, group]: scene.groups) {
        glGenTextures(1, &texture);
        textures_albedo[name] = texture;
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

        if (group.material.albedo.empty()) {
            std::cout << "no texture!" << std::endl;
            continue;
        }
        int x, y, n;
        unsigned char *texture_data = stbi_load(group.material.albedo.c_str(), &x, &y, &n, 4);
        if (texture_data == nullptr) {
            std::cout << "Cannot load texture " << group.material.albedo.c_str() << ":" << stbi_failure_reason() << std::endl;
            throw std::runtime_error("Cannot load texture");
        }
        if (n == 4)
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, x, y, 0, GL_RGBA, GL_UNSIGNED_BYTE, texture_data);
        else if (n == 3)
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, x, y, 0, GL_RGBA, GL_UNSIGNED_BYTE, texture_data);
        glGenerateMipmap(GL_TEXTURE_2D);

//        if (group.material.transparency.empty()) {
////            std::cout << "no transparency!" << std::endl;
//        }
        stbi_image_free(texture_data);


        glGenTextures(1, &texture);
        textures_transparency[name] = texture;
        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

        if (group.material.transparency.empty()) {
//            std::cout << "no transparency!" << std::endl;
            continue;
        }
        texture_data = stbi_load(group.material.transparency.c_str(), &x, &y, &n, 1);
        if (texture_data == nullptr) {
            std::cout << "Cannot load transparency " << group.material.transparency.c_str() << ":" << stbi_failure_reason() << std::endl;
            throw std::runtime_error("Cannot load transparency");
        }
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, x, y, 0, GL_RED, GL_UNSIGNED_BYTE, texture_data);
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

        auto now = std::chrono::high_resolution_clock::now();
        float dt = std::chrono::duration_cast<std::chrono::duration<float>>(now - last_frame_start).count();
        last_frame_start = now;
        time += dt;

        float camera_speed = 6.f;
        if (button_down[SDLK_LSHIFT])
            camera_speed *= 5;
        if (button_down[SDLK_LCTRL])
            camera_speed /= 3;

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


        glViewport(0, 0, width, height);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glEnable(GL_DEPTH_TEST);
        glEnable(GL_CULL_FACE);
        glCullFace(GL_BACK);

        float near = 0.1f;
        float far = 500.f;

        camera_pitch = std::max(-glm::pi<float>() / 2 + 0.01f, std::min(glm::pi<float>() / 2 - 0.01f, camera_pitch));

        glm::vec3 direction;
        direction.x = cos(camera_yaw) * cos(camera_pitch); // Note that we convert the angle to radians first
        direction.y = sin(camera_pitch);
        direction.z = sin(camera_yaw) * cos(camera_pitch);
        camera_front = glm::normalize(direction);

        glm::mat4 model(1.f);

        glm::mat4 view(1.f);

        view = glm::lookAt(camera_pos, camera_pos + camera_front, camera_up);

        float aspect = (float) height / (float) width;
        glm::mat4 projection = glm::perspective(glm::pi<float>() / 3.f, 1.f / aspect, near, far);

        glm::vec3 camera_position = glm::vec3(glm::inverse(view) * glm::vec4(0.f, 0.f, 0.f, 1.f));

        glm::vec3 sun_direction = glm::normalize(glm::vec3(std::sin(time * 0.5f), 3.f, std::cos(time * 0.5f)));

        glUseProgram(program);

        glUniformMatrix4fv(model_location, 1, GL_FALSE, reinterpret_cast<float *>(&model));
        glUniformMatrix4fv(view_location, 1, GL_FALSE, reinterpret_cast<float *>(&view));
        glUniformMatrix4fv(projection_location, 1, GL_FALSE, reinterpret_cast<float *>(&projection));
        glUniform3fv(camera_position_location, 1, (float *) (&camera_position));
        glUniform3f(sun_color_location, 1.f, 1.f, 1.f);
        glUniform3fv(sun_direction_location, 1, reinterpret_cast<float *>(&sun_direction));

        glBindVertexArray(scene_vao);
        for (auto &[name, group]: scene.groups) {
            if  (group.material.albedo.empty()) {
                glUniform1i(albedo_location, 0);
            }
            else {
                glActiveTexture(GL_TEXTURE1);
                glBindTexture(GL_TEXTURE_2D, textures_albedo[name]);
                glUniform1i(albedo_location, 1);
            }

            if  (group.material.transparency.empty()) {
                glUniform1i(solid_location, 1);
                glUniform1i(transparency_location, 1);
            }
            else {
                glUniform1i(solid_location, 0);
                glActiveTexture(GL_TEXTURE2);
                glBindTexture(GL_TEXTURE_2D, textures_transparency[name]);
                glUniform1i(transparency_location, 2);
            }


            glUniform3fv(glossiness_location, 1, reinterpret_cast<float *>(&group.material.glossiness));
            glUniform1f(roughness_location, group.material.roughness);

            glDrawElements(GL_TRIANGLES, group.count, GL_UNSIGNED_INT, (uint32_t*)nullptr + group.offset);
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
