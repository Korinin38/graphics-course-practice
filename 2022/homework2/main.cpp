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
    std::map<std::string, int> textures_albedo;
    std::map<std::string, int> textures_transparency;
    int tex_num = 0;
    for (auto &[name, group]: scene.groups) {
        std::cout << name << " " << tex_num << std::endl;
        GLuint texture;
        glGenTextures(1, &texture);
        textures_albedo[name] = tex_num++;
        glActiveTexture(GL_TEXTURE0 + textures_albedo[name]);
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

        int x, y, n;
        unsigned char *texture_data = stbi_load(group.material.albedo.c_str(), &x, &y, &n, 4);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, x, y, 0, GL_RGB, GL_UNSIGNED_BYTE, texture_data);
        glGenerateMipmap(GL_TEXTURE_2D);
        stbi_image_free(texture_data);
    }

    // Uniforms - vertex
    GLuint model_location = glGetUniformLocation(program, "model");
    GLuint view_location = glGetUniformLocation(program, "view");
    GLuint projection_location = glGetUniformLocation(program, "projection");

    // Uniforms - fragment
    GLuint camera_position_location = glGetUniformLocation(program, "camera_position");
    GLuint albedo_location = glGetUniformLocation(program, "albedo");
    GLuint sun_direction_location = glGetUniformLocation(program, "sun_direction");
    GLuint sun_color_location = glGetUniformLocation(program, "sun_color");
    GLuint glossiness_location = glGetUniformLocation(program, "glossiness");
    GLuint roughness_location = glGetUniformLocation(program, "roughness");

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

    for (auto &[name, group]: scene.groups) {
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, group.indices.size() * sizeof(group.indices[0]), group.indices.data(),
                     GL_STATIC_DRAW);
        // lazy first element
        break;
    }

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(obj_parser::obj_data::vertex), nullptr);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(obj_parser::obj_data::vertex), (void *) (12));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(obj_parser::obj_data::vertex), (void *) (24));

    std::map<SDL_Keycode, bool> button_down;

    float camera_distance = 1.5f;
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

        float cam_speed = 1.f;
        if (button_down[SDLK_LSHIFT])
            cam_speed *= 3;
        if (button_down[SDLK_LCTRL])
            cam_speed /= 3;

        if (button_down[SDLK_w] or button_down[SDLK_UP])
            camera_distance -= 4.f * cam_speed * dt;
        if (button_down[SDLK_s] or button_down[SDLK_DOWN])
            camera_distance += 4.f * cam_speed * dt;

        if (button_down[SDLK_a] or button_down[SDLK_LEFT])
            camera_yaw += 2.f * dt;
        if (button_down[SDLK_d] or button_down[SDLK_RIGHT])
            camera_yaw -= 2.f * dt;

        if (button_down[SDLK_z])
            camera_pitch += 2.f * dt;
        if (button_down[SDLK_c])
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
        float far = 100.f;

        glm::mat4 model(1.f);

        glm::mat4 view(1.f);
        view = glm::translate(view, {0.f, 0.f, -camera_distance});
        view = glm::rotate(view, glm::pi<float>() / 6.f, {1.f, 0.f, 0.f});
        view = glm::rotate(view, camera_pitch, {1.f, 0.f, 0.f});
        view = glm::rotate(view, camera_yaw, {0.f, 1.f, 0.f});
        view = glm::rotate(view, camera_roll, {0.f, 0.f, 1.f});
        view = glm::translate(view, {0.f, -0.5f, 0.f});

        float aspect = (float) height / (float) width;
        glm::mat4 projection = glm::perspective(glm::pi<float>() / 3.f, 1.f / aspect, near, far);

        glm::vec3 camera_position = glm::vec3(glm::inverse(view) * glm::vec4(0.f, 0.f, 0.f, 1.f));

        glm::vec3 sun_direction = glm::normalize(glm::vec3(std::sin(time * 0.5f), 2.f, std::cos(time * 0.5f)));

        glUseProgram(program);

        glUniformMatrix4fv(model_location, 1, GL_FALSE, reinterpret_cast<float *>(&model));
        glUniformMatrix4fv(view_location, 1, GL_FALSE, reinterpret_cast<float *>(&view));
        glUniformMatrix4fv(projection_location, 1, GL_FALSE, reinterpret_cast<float *>(&projection));
        glUniform3fv(camera_position_location, 1, (float *) (&camera_position));
        glUniform3f(sun_color_location, 1.f, 1.f, 1.f);
        glUniform3fv(sun_direction_location, 1, reinterpret_cast<float *>(&sun_direction));

        glBindVertexArray(scene_vao);
        for (auto &[name, group]: scene.groups) {
            glUniform1i(albedo_location, textures_albedo[name]);
            glUniform3fv(glossiness_location, 1, reinterpret_cast<float *>(&group.material.glossiness));
            glUniform1f(roughness_location, group.material.roughness);

            glBufferData(GL_ELEMENT_ARRAY_BUFFER, group.indices.size() * sizeof(group.indices[0]), group.indices.data(),
                         GL_STATIC_DRAW);
            glDrawElements(GL_TRIANGLES, group.indices.size(), GL_UNSIGNED_INT, nullptr);
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
