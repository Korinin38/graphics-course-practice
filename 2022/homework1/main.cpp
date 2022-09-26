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

std::string to_string(std::string_view str) {
    return {str.begin(), str.end()};
}

void sdl2_fail(std::string_view message) {
    throw std::runtime_error(to_string(message) + SDL_GetError());
}

void glew_fail(std::string_view message, GLenum error) {
    throw std::runtime_error(to_string(message) + reinterpret_cast<const char*>(glewGetErrorString(error)));
}

std::string readFile(const std::string& file_name, bool verbose = false) {
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

GLuint create_shader(GLenum type, const char* source) {
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

struct vec3 {
    float x;
    float y;
    float z;
};

class Configurer {
    //
    float max_value = 100;
    int isolines = 10;
    int _grid_x = 100;
    int _grid_y = 100;

    Configurer() = default;

public:
    const float x0 = 0;
    const float x1 = 0;
    const float y0 = 0;
    const float y1 = 0;

    Configurer(Configurer const&) = delete;

    void operator=(Configurer const&) = delete;

    static Configurer& getInstance() {
        static Configurer instance;
        return instance;
    }

    int W() const { return _grid_x; }

    int H() const { return _grid_y; }
};
Configurer& config = Configurer::getInstance();

float f(float x, float y, float t) {
    return x * x + y * y + sin(t) * 10;
}

void calculate(std::vector<std::vector<float>>& vec, float time, float (* func)(float, float, float)) {
    if (vec.size() != config.W() || vec[0].size() != config.H()) {
        vec.resize(config.W(), std::vector<float>(config.H()));
    }

    for (int i = 0; i < vec.size(); ++i) {
        for (int j = 0; j < vec[i].size(); ++j) {
            float x = config.x0 + (config.x1 - config.x0) * (float)i / (float)config.W();
            float y = config.y0 + (config.y1 - config.y0) * (float)j / (float)config.W();
            vec[i][j] = func(x, y, time);
        }
    }

}

int main() try {
    if (SDL_Init(SDL_INIT_VIDEO) != 0)
        sdl2_fail("SDL_Init: ");

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1);
    SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 4);

    SDL_Window* window = SDL_CreateWindow("Graphics course practice 3",
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

    glClearColor(.5f, .8f, 1.f, 0.f);

    std::string vertex_shader_source = readFile("shaders/shader.vert");
    std::string fragment_shader_source = readFile("shaders/shader.frag");

    auto vertex_shader = create_shader(GL_VERTEX_SHADER, vertex_shader_source.data());
    auto fragment_shader = create_shader(GL_FRAGMENT_SHADER, fragment_shader_source.data());
    auto program = create_program(vertex_shader, fragment_shader);

    float time = 0.f;

    // Number of points calculated in every row, as well as the number of said rows
    // Total number of points calculated = grid_density ^ 2

    GLint view_location = glGetUniformLocation(program, "view");

    std::vector<std::vector<float>> values(config.W(), std::vector<float>(config.H()));
    calculate(values, 0, f);
    std::cout << "Done" << std::endl;

    auto last_frame_start = std::chrono::high_resolution_clock::now();

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
                case SDL_MOUSEBUTTONDOWN:
                    if (event.button.button == SDL_BUTTON_LEFT) {}
                    else if (event.button.button == SDL_BUTTON_RIGHT) {}
                    break;
                case SDL_KEYDOWN:
                    if (event.key.keysym.sym == SDLK_LEFT) {}
                    else if (event.key.keysym.sym == SDLK_RIGHT) {}
                    else if (event.key.keysym.sym == SDLK_b) {}
                    break;
            }

        if (!running)
            break;

        auto now = std::chrono::high_resolution_clock::now();
        float dt = std::chrono::duration_cast<std::chrono::duration<float>>(now - last_frame_start).count();
        last_frame_start = now;
        time += dt;

        glClear(GL_COLOR_BUFFER_BIT);

        float view[16] =
                {
                        2.f / width, 0.f, 0.f, -1.f,
                        0.f, 2.f / height, 0.f, -1.f,
                        0.f, 0.f, 1.f, 0.f,
                        0.f, 0.f, 0.f, 1.f,
                };

        glUseProgram(program);

//        glBindVertexArray(bezier_vao);
        glLineWidth(3.f);
//        glDrawArrays(GL_LINE_STRIP, 0, 0);
        glUniformMatrix4fv(view_location, 1, GL_TRUE, view);

        SDL_GL_SwapWindow(window);
    }

    SDL_GL_DeleteContext(gl_context);
    SDL_DestroyWindow(window);
}
catch (std::exception const& e) {
    std::cerr << e.what() << std::endl;
    return EXIT_FAILURE;
}
