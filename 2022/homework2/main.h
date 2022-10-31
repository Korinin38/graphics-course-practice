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
    unsigned int _isolines = 3;
    unsigned int _grid_x = 30;
    unsigned int _grid_y = 30;
    float wait = 0;

    Configurer() = default;

public:
    const std::uint32_t PRIMITIVE_RESTART_INDEX = 1e7;
    const float X0 = -100;
    const float X1 = 100;
    const float Y0 = -100;
    const float Y1 = 100;
    const float MAX_VALUE = 10000;
    const unsigned int MAX_ISOLINES = 30;
    const unsigned int MAX_GRID = 1000;

    Configurer(Configurer const&) = delete;

    void operator=(Configurer const&) = delete;

    static Configurer& getInstance() {
        static Configurer instance;
        return instance;
    }

    void W(unsigned int grid_x, float dt) {
        if (wait > 0) {
            wait -= dt;
            return;
        }
        _grid_x = std::max(1u, std::min(grid_x, MAX_GRID));
        wait = 0.01;
    }

    void H(unsigned int grid_y, float dt) {
        if (wait > 0) {
            wait -= dt;
            return;
        }
        _grid_y = std::max(1u, std::min(grid_y, MAX_GRID));
        wait = 0.01;
    }

    unsigned int W() const { return _grid_x; }

    unsigned int H() const { return _grid_y; }

    void isolines(unsigned int iso_num, float dt) {
        if (wait > 0) {
            wait -= dt;
            return;
        }
        if (iso_num < 2 || iso_num > MAX_ISOLINES) { return; }
        _isolines = iso_num;
        wait = 0.06;
    }

    unsigned int isolines() const {
        return _isolines;
    }
};
Configurer& config = Configurer::getInstance();

std::uint32_t grid_index(int i, int j) {
    return i * (config.H() + 1) + j;
}

std::uint32_t iso_index(int i, int j, int edge) {
    if (i == config.W() - 1 && edge == 1) {
        return ((i + 1) * config.H()) * 3 + (i + 1) + j;
    }
    switch (edge) {
        case 0:
            return (i * config.H() + j) * 3 + i;
        case 1:
            return ((i + 1) * config.H() + j) * 3 + (i + 1) + 2;
        case 2:
            return (i * config.H() + j) * 3 + i + 1;
        case 3:
            return (i * config.H() + j) * 3 + i + 2;
        case 4:
            return (i * config.H() + j + 1) * 3 + i;
        default:
            throw std::runtime_error("Error while picking edge");
    }
}

void set_grid_indices(std::vector<std::uint32_t>& indices) {
    // W columns of H+1 vertices, plus W primitive-restarts
    indices.resize(config.W() * (config.H() * 2 + 3));

    int count = 0;
    // vertices are enumerated this way:
    // [ 0, 3, 6,
    //   1, 4, 7,
    //   2, 5, 8,...
    for (int i = 0; i < config.W(); ++i) {
        // Traversing columns, two at a time
        for (int j = 0; j < config.H() + 1; ++j) {
            indices[count++] = grid_index(i, j);
            indices[count++] = grid_index(i + 1, j);
        }
        indices[count++] = config.PRIMITIVE_RESTART_INDEX;
    }
}

void place_grid(std::vector<vec2>& vec, int width, int height, bool scaled_up = false) {
    vec.resize((config.W() + 1) * (config.H() + 1));

    int limit = scaled_up ? std::max(width, height) : std::min(width, height);
    float dx = (float) (scaled_up ? -std::max(limit - width, 0) : std::max(width - limit, 0)) / 2.0f;
    float dy = (float) (scaled_up ? -std::max(limit - height, 0) : std::max(height - limit, 0)) / 2.0f;

    for (int i = 0; i < config.W() + 1; ++i) {
        for (int j = 0; j < config.H() + 1; ++j) {
            float x = float(limit) * (float) i / (float) config.W() + dx;
            float y = float(limit) * (float) j / (float) config.H() + dy;
            vec[grid_index(i, j)] = {x, y};
        }
    }
}

unsigned char variation(float val1, float val2, float val3, float iso_val) {
    unsigned char res = 0;
    if (iso_val < val1) res |= 0b001;
    if (iso_val < val2) res |= 0b010;
    if (iso_val < val3) res |= 0b100;
    return res;
}

typedef struct point {
    float val;
    vec2 pos;
} point;

vec2 interpolate(point v1, point v2, float iso_val) {
    // returns point between v1 and v2 that corresponds to iso_val,
    // if possible. Undefined otherwise
    if (v2.pos.x < v1.pos.x || v2.pos.y < v1.pos.y)  std::swap(v2, v1);
    float q = (iso_val - v1.val) / (v2.val - v1.val);
    if (q < 0 or q > 1)
        return {0, 0};
    return {v2.pos.x * q + v1.pos.x * (1 - q), v2.pos.y * q + v1.pos.y * (1 - q)};
}

void parse_configuration(std::vector<std::uint32_t>& indices, std::vector<vec2>& pos, int i, int j, const int edge,
                         unsigned char configuration,
                         bool top, const point v[],
                         float iso_value) {
    switch (configuration) {
        case 1:
        case 6:
            if (top) {
                pos[iso_index(i, j, edge)] = interpolate(v[0], v[1], iso_value);
                indices.push_back(iso_index(i, j, edge));
            }
            pos[iso_index(i, j, edge + 2)] = interpolate(v[0], v[2], iso_value);
            indices.push_back(iso_index(i, j, edge + 2));
            break;
        case 2:
        case 5:
            if (top) {
                pos[iso_index(i, j, edge)] = interpolate(v[0], v[1], iso_value);
                indices.push_back(iso_index(i, j, edge));
            }
            pos[iso_index(i, j, edge + 1)] = interpolate(v[1], v[2], iso_value);
            indices.push_back(iso_index(i, j, edge + 1));
            indices.push_back(config.PRIMITIVE_RESTART_INDEX);
            break;
        case 3:
        case 4:
            if (!indices.empty() && indices.back() != config.PRIMITIVE_RESTART_INDEX)
                indices.push_back(config.PRIMITIVE_RESTART_INDEX);
            pos[iso_index(i, j, edge + 1)] = interpolate(v[1], v[2], iso_value);
            indices.push_back(iso_index(i, j, edge + 1));
            pos[iso_index(i, j, edge + 2)] = interpolate(v[0], v[2], iso_value);
            indices.push_back(iso_index(i, j, edge + 2));
            break;
        case 0:
        case 7:
            if (!indices.empty() && indices.back() != config.PRIMITIVE_RESTART_INDEX)
                indices.push_back(config.PRIMITIVE_RESTART_INDEX);
            break;
    }
}

void calculate_isolines(std::vector<std::vector<vec2>>& pos,
                        std::vector<std::vector<std::uint32_t>>& indices,
                        const std::vector<float>& vals,
                        int width, int height, bool scaled_up = false) {
    // Vals must be correctly filled!
    if (vals.size() != (config.W() + 1) * (config.H() + 1))
        throw std::runtime_error("'vals' must be correctly filled before calculating isolines");

    int limit = scaled_up ? std::max(width, height) : std::min(width, height);
    float dx = (float) (scaled_up ? -std::max(limit - width, 0) : std::max(width - limit, 0)) / 2.0f;
    float dy = (float) (scaled_up ? -std::max(limit - height, 0) : std::max(height - limit, 0)) / 2.0f;

    pos.resize(config.isolines());
    indices.resize(config.isolines());

    // First pos is always a boarder of graph
    pos[0].resize(8);
    pos[0][0] = {dx, dy};
    pos[0][1] = {dx, float(limit) / 2.0f + dy};
    pos[0][2] = {dx, float(limit) + dy};
    pos[0][3] = {float(limit) / 2.0f + dx, float(limit) + dy};
    pos[0][4] = {float(limit) + dx, float(limit) + dy};
    pos[0][5] = {float(limit) + dx, float(limit) / 2.0f + dy};
    pos[0][6] = {float(limit) + dx, dy};
    pos[0][7] = {float(limit) / 2.0f + dx, dy};
    indices[0] = {0, 1, 2, 3, 4, 5, 6, 7, 0};

    for (int cur_isoline = 1; cur_isoline < pos.size(); ++cur_isoline) {
        pos[cur_isoline] = {{0, 0}};
        pos[cur_isoline].resize(config.W() * config.H() * 3 + config.W() + config.H());
        indices[cur_isoline].clear();
        float iso_value = (config.MAX_VALUE * 2) * cur_isoline / config.isolines() - config.MAX_VALUE;
        for (int i = 0; i < config.W(); ++i) {
            for (int j = 0; j < config.H(); ++j) {
                // Edges are counted as (grid_index pf up-leftmost vertex) * 3 +
                //   --0--
                //   |\
                //   2 1
                //   |  \

                // Edges here:
                //   --0--
                //   |\  |
                //   3 2 1
                //   |  \|
                //   --4--

                //  0---1
                //  | \ |
                //  3---2

                // first triangle
                const point v1[3] = {{vals[grid_index(i, j)],     {1.0f * limit * i / config.W() + dx,
                                                                   1.0f * limit * j / config.H() + dy}},
                                     {vals[grid_index(i + 1, j)], {1.0f * limit * (i + 1) / config.W() + dx,
                                                                   1.0f * limit * j / config.H() + dy}},
                                     {vals[grid_index(i + 1, j + 1)],
                                                                  {1.0f * limit * (i + 1) / config.W() + dx,
                                                                   1.0f * limit * (j + 1) / config.H() + dy}}};

                // second triangle
                const point v2[3] = {v1[2],
                                     v1[0],
                                     {vals[grid_index(i, j + 1)],
                                            {1.0f * limit * i / config.W() + dx,
                                             1.0f * limit * (j + 1) / config.H() + dy}}};

                unsigned char var1 = variation(v1[0].val, v1[1].val, v1[2].val, iso_value);
                unsigned char var2 = variation(v2[0].val, v2[1].val, v2[2].val, iso_value);

                parse_configuration(indices[cur_isoline], pos[cur_isoline], i, j, 0, var1, (j == 0), v1, iso_value);
                parse_configuration(indices[cur_isoline], pos[cur_isoline], i, j, 2, var2, false, v2, iso_value);
            }
            indices[cur_isoline].push_back(config.PRIMITIVE_RESTART_INDEX);
        }
    }
}

void calculate_grid(std::vector<float>& vec, float time,
                    float (* func)(float, float, float)) {
    vec.resize((config.W() + 1) * (config.H() + 1));

    for (int i = 0; i < config.W() + 1; ++i) {
        for (int j = 0; j < config.H() + 1; ++j) {
            float x = config.X0 + (config.X1 - config.X0) * (float) i / (float) config.W();
            float y = config.Y0 + (config.Y1 - config.Y0) * (float) j / (float) config.H();
            vec[grid_index(i, j)] = func(x, y, time);
        }
    }
}

void set_buffers_grid(GLuint& vao, GLuint& pos_vbo, GLuint& val_vbo, GLuint& ebo,
                      std::vector<vec2> const& pos,
                      std::vector<float> const& values,
                      std::vector<std::uint32_t> const& indices) {
    glBindVertexArray(vao);

    glBindBuffer(GL_ARRAY_BUFFER, pos_vbo);
    glBufferData(GL_ARRAY_BUFFER, pos.size() * sizeof(vec2), pos.data(), GL_DYNAMIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, (void*) (0));


    glBindBuffer(GL_ARRAY_BUFFER, val_vbo);
    glBufferData(GL_ARRAY_BUFFER, values.size() * sizeof(float), values.data(), GL_STREAM_DRAW);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 1, GL_FLOAT, GL_FALSE, 0, (void*) (0));

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(std::uint32_t), indices.data(), GL_DYNAMIC_DRAW);
}

void set_buffers_iso(GLuint& vao, GLuint& vbo, GLuint& ebo,
                     std::vector<vec2> const& pos,
                     std::vector<std::uint32_t> const& indices) {
    glBindVertexArray(vao);

    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, pos.size() * sizeof(vec2), pos.data(), GL_STREAM_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, (void*) (0));

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(std::uint32_t), indices.data(),
                 GL_STREAM_DRAW);
}

void primitive_button_handler(std::map<SDL_Keycode, bool>& button_down,
                              float dt,
                              bool& update_pos,
                              bool& update_quality,
                              bool& scale_up,
                              bool& hold_b,
                              bool& pause,
                              int& cur_func) {
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
        config.W(300, dt);
        config.H(300, dt);

        update_pos = true;
        update_quality = true;
    }
    if (button_down[SDLK_4]) {
        config.W(500, dt);
        config.H(500, dt);

        update_pos = true;
        update_quality = true;
    }
    if (button_down[SDLK_5]) {
        config.W(1000, dt);
        config.H(1000, dt);

        update_pos = true;
        update_quality = true;
    }

    if (button_down[SDLK_b] && !hold_b) {
        hold_b = true;
        scale_up ^= true;
        update_pos = true;
        update_quality = true;
    } else if (!button_down[SDLK_b]) {
        hold_b = false;
    }
    if (button_down[SDLK_p] && !pause) {
        pause = true;
    } else if (!button_down[SDLK_p]) {
        pause = false;
    }

    if (button_down[SDLK_EQUALS]) {
        config.W(config.W() + 1, dt);
        config.H(config.H() + 1, dt);

        update_pos = true;
        update_quality = true;
    }
    if (button_down[SDLK_MINUS]) {
        config.W(std::max(config.W() - 1, (unsigned) 1), dt);
        config.H(std::max(config.H() - 1, (unsigned) 1), dt);

        update_pos = true;
        update_quality = true;
    }

    if (button_down[SDLK_LEFT]) {
        config.isolines(config.isolines() - 1, dt);

        update_pos = true;
        update_quality = true;
    }
    if (button_down[SDLK_RIGHT]) {
        config.isolines(config.isolines() + 1, dt);

        update_pos = true;
        update_quality = true;
    }
    if (button_down[SDLK_o]) {
        config.isolines(2, dt);

        update_pos = true;
        update_quality = true;
    }

    if (button_down[SDLK_z]) {
        cur_func = 0;
    }
    if (button_down[SDLK_x]) {
        cur_func = 1;
    }
    if (button_down[SDLK_c]) {
        cur_func = 2;
    }
    if (button_down[SDLK_v]) {
        cur_func = 3;
    }
}