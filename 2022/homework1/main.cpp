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

struct vec2 {
    float x;
    float y;
};

class Configurer {
    //
    int isolines = 10;
    int _grid_x = 100;
    int _grid_y = 100;
    float wait = 0;

    Configurer() = default;

public:
    const std::uint32_t PRIMITIVE_RESTART_INDEX = 2e9;
    const float X0 = -100;
    const float X1 = 100;
    const float Y0 = -100;
    const float Y1 = 100;
    const float MAX_VALUE = 10000;
    const int MAX_GRID = 10000;

    Configurer(Configurer const&) = delete;

    void operator=(Configurer const&) = delete;

    static Configurer& getInstance() {
        static Configurer instance;
        return instance;
    }

    void W(int grid_x, float dt) {
        if (wait > 0) {
            wait -= dt;
            return;
        }
        _grid_x = std::max(1, std::min(grid_x, MAX_GRID));
        wait = 0.01;
    }

    void H(int grid_y, float dt) {
        if (wait > 0) {
            wait -= dt;
            return;
        }
        _grid_y = std::max(1, std::min(grid_y, MAX_GRID));
        wait = 0.01;
    }

    int W() const { return _grid_x; }

    int H() const { return _grid_y; }
};
Configurer& config = Configurer::getInstance();

float f(float x, float y, float t) {
    return x * x - y * y + sin(t) * 3000;
}

void set_indices(std::vector<std::uint32_t>& indices) {
    // W columns of H+1 vertices, plus W primitive-restarts
    int size =  config.W() * (config.H() * 2 + 3);
    if (indices.size() != size) {
        indices.resize(size);
    }

    int count = 0;
    // vertices are enumerated this way:
    // [ 0, 3, 6,
    //   1, 4, 7,
    //   2, 5, 8,...
    for (int i = 0; i < config.W(); ++i) {
        // Traversing columns, two at a time
        for (int j = 0; j < config.H() + 1; ++j) {
            indices[count++]  = i * (config.H() + 1) + j;
            indices[count++]  = (i + 1) * (config.H() + 1) + j;
        }
        indices[count++] = config.PRIMITIVE_RESTART_INDEX;
    }
}

void place_grid(std::vector<vec2>& vec, int width, int height) {
    if (vec.size() != (config.W() + 1) * (config.H() + 1))
        vec.resize((config.W() + 1) * (config.H() + 1));

    for (int i = 0; i < config.W() + 1; ++i) {
        for (int j = 0; j < config.H() + 1; ++j) {
            float x = float(width) * (float)i / (float)config.W();
            float y = float(height) * (float)j / (float)config.H();
            vec[i * (config.H() + 1) + j] = {x, y};
        }
    }
}

void calculate(std::vector<float>& vec, float time, float (* func)(float, float, float)) {
    if (vec.size() != (config.W() + 1) * (config.H() + 1)) {
        vec.resize((config.W() + 1) * (config.H() + 1));
    }

    for (int i = 0; i < config.W() + 1; ++i) {
        for (int j = 0; j < config.H() + 1; ++j) {
            float x = config.X0 + (config.X1 - config.X0) * (float)i / (float)config.W();
            float y = config.Y0 + (config.Y1 - config.Y0) * (float)j / (float)config.H();
            vec[i * (config.H() + 1) + j] = func(x, y, time);
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
    glEnable(GL_PRIMITIVE_RESTART);
    glPrimitiveRestartIndex(config.PRIMITIVE_RESTART_INDEX);

    float time = 0.f;

    // Number of points calculated in every row, as well as the number of said rows
    // Total number of points calculated = grid_density ^ 2

    GLint view_location = glGetUniformLocation(program, "view");
    GLint value_limit = glGetUniformLocation(program, "max_value");

    std::vector<float> values((config.W() + 1) * (config.H() + 1));
    std::vector<vec2> pos((config.W() + 1) * (config.H() + 1));
    std::vector<std::uint32_t> indices(10);
    calculate(values, 0, f);
    place_grid(pos, width, height);
    set_indices(indices);

    GLuint grid_vao, grid_pos_vbo, grid_val_vbo, grid_ebo;
    glGenVertexArrays(1, &grid_vao);
    glBindVertexArray(grid_vao);

    glGenBuffers(1, &grid_pos_vbo);
    glBindBuffer(GL_ARRAY_BUFFER, grid_pos_vbo);
    glBufferData(GL_ARRAY_BUFFER, pos.size() * sizeof(vec2), pos.data(), GL_DYNAMIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, (void*)(0));


    glGenBuffers(1, &grid_val_vbo);
    glBindBuffer(GL_ARRAY_BUFFER, grid_val_vbo);
    glBufferData(GL_ARRAY_BUFFER, values.size() * sizeof(float), values.data(), GL_STREAM_DRAW);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 1, GL_FLOAT, GL_FALSE, 0, (void*)(0));

    glGenBuffers(1, &grid_ebo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, grid_ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(std::uint32_t), indices.data(), GL_DYNAMIC_DRAW);

    auto last_frame_start = std::chrono::high_resolution_clock::now();

    std::map<SDL_Keycode, bool> button_down;
    bool update_pos = true;
    bool update_quality = true;

    bool running = true;
    while (running) {
        auto now = std::chrono::high_resolution_clock::now();
        float dt = std::chrono::duration_cast<std::chrono::duration<float>>(now - last_frame_start).count();

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
                            place_grid(pos, width, height);
                            update_pos = true;
                            break;
                    }
                    break;
                case SDL_MOUSEBUTTONDOWN:
                    if (event.button.button == SDL_BUTTON_LEFT) {}
                    else if (event.button.button == SDL_BUTTON_RIGHT) {}
                    break;
                case SDL_KEYDOWN:
                    button_down[event.key.keysym.sym] = true;
                    break;
                case SDL_KEYUP:
                    button_down[event.key.keysym.sym] = false;
                    break;
            }

        if (button_down[SDLK_0]) {
            config.W(1, dt);
            config.H(1, dt);

            update_pos = true;
            update_quality = true;
        }
        if (button_down[SDLK_1]) {
            config.W(10, dt);
            config.H(10, dt);

            update_pos = true;
            update_quality = true;
        }
        if (button_down[SDLK_2]) {
            config.W(100, dt);
            config.H(100, dt);

            update_pos = true;
            update_quality = true;
        }
        if (button_down[SDLK_3]) {
            config.W(500, dt);
            config.H(500, dt);

            update_pos = true;
            update_quality = true;
        }
        if (button_down[SDLK_4]) {
            config.W(1000, dt);
            config.H(1000, dt);

            update_pos = true;
            update_quality = true;
        }
        if (button_down[SDLK_5]) {
            config.W(5000, dt);
            config.H(5000, dt);

            update_pos = true;
            update_quality = true;
        }

        if (button_down[SDLK_EQUALS]) {
            config.W(config.W() + 1, dt);
            config.H(config.H() + 1, dt);

            update_pos = true;
            update_quality = true;
        }
        if (button_down[SDLK_MINUS]) {
            config.W(std::max(config.W() - 1, 1), dt);
            config.H(std::max(config.H() - 1, 1), dt);

            update_pos = true;
            update_quality = true;
        }

        if (!running)
            break;

        last_frame_start = now;
        time += dt;

        calculate(values, time, f);
        glBindBuffer(GL_ARRAY_BUFFER, grid_val_vbo);
        glBufferData(GL_ARRAY_BUFFER, values.size() * sizeof(float), values.data(), GL_STREAM_DRAW);
        if (update_pos) {
            update_pos = false;
            place_grid(pos, width, height);
            glBindBuffer(GL_ARRAY_BUFFER, grid_pos_vbo);
            glBufferData(GL_ARRAY_BUFFER, pos.size() * sizeof(vec2), pos.data(), GL_DYNAMIC_DRAW);
        }
        if (update_quality) {
            update_quality = false;
            set_indices(indices);
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, grid_ebo);
            glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(std::uint32_t), indices.data(), GL_DYNAMIC_DRAW);
        }

        glClear(GL_COLOR_BUFFER_BIT);

        float view[16] =
                {
                        2.f / width, 0.f, 0.f, -1.f,
                        0.f, 2.f / height, 0.f, -1.f,
                        0.f, 0.f, 1.f, 0.f,
                        0.f, 0.f, 0.f, 1.f,
                };

        glUseProgram(program);

        glDrawElements(GL_TRIANGLE_STRIP, indices.size(), GL_UNSIGNED_INT, (void*)(0));
        glDrawElements(GL_LINE_STRIP, indices.size(), GL_UNSIGNED_INT, (void*)(0));
        glUniformMatrix4fv(view_location, 1, GL_TRUE, view);
        glUniform1f(value_limit, config.MAX_VALUE);

        SDL_GL_SwapWindow(window);
    }

    SDL_GL_DeleteContext(gl_context);
    SDL_DestroyWindow(window);
}
catch (std::exception const& e) {
    std::cerr << e.what() << std::endl;
    return EXIT_FAILURE;
}
