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

std::string to_string(std::string_view str)
{
	return std::string{str.begin(), str.end()};
}

void sdl2_fail(std::string_view message)
{
	throw std::runtime_error(to_string(message) + SDL_GetError());
}

void glew_fail(std::string_view message, GLenum error)
{
	throw std::runtime_error(to_string(message) + reinterpret_cast<const char *>(glewGetErrorString(error)));
}

const char vertex_source[] = R"(#version 330 core
const vec2 VERTICES[3] = vec2[3](
    vec2(0.0, 0.0),
    vec2(1.0, 0.0),
    vec2(0.0, 1.0)
);
out vec2 pos;
void main()
{
    gl_Position = vec4(VERTICES[gl_VertexID], 0.0, 1.0);
    pos = vec2(gl_Position[0], gl_Position[1]);
}
)";
const char fragment_source[] = R"(#version 330 core
layout (location = 0) out vec4 out_color;
in vec2 pos;
const float scale = 10;
void main()
{
    // vec4(R, G, B, A)
    int col = int(floor(pos[0] * scale) + floor(pos[1] * scale)) % 2;
    out_color = vec4(col, col, col, 1.0);
}
)";

GLuint create_shader(GLenum shader_type, const char* shader_source) {
    GLuint shader_id = glCreateShader(shader_type);
    glShaderSource(shader_id, 1, &shader_source, nullptr);
    glCompileShader(shader_id);
    GLint compiled;
    glProvokingVertex(GL_LAST_VERTEX_CONVENTION);
    glGetShaderiv(shader_id, GL_COMPILE_STATUS, &compiled);
    if (not compiled) {
        GLint log_length;
        glGetShaderiv(shader_id, GL_INFO_LOG_LENGTH, &log_length);
        std::string info_log(log_length, '\0');
        glGetShaderInfoLog(shader_id, log_length, &log_length, info_log.data());
        throw std::runtime_error(info_log);
    }
    return shader_id;
}

GLuint create_program(GLuint vertex_shader, GLuint fragment_shader) {
    GLuint program_id = glCreateProgram();
    glAttachShader(program_id, vertex_shader);
    glAttachShader(program_id, fragment_shader);
    glLinkProgram(program_id);
    GLint linked;
    glGetProgramiv(program_id, GL_LINK_STATUS, &linked);
    if (not linked) {
        GLint log_length;
        glGetShaderiv(program_id, GL_INFO_LOG_LENGTH, &log_length);
        std::string info_log(log_length, '\0');
        glGetProgramInfoLog(program_id, log_length, &log_length, info_log.data());
        throw std::runtime_error(info_log);
    }
    return program_id;
}

int main() try
{
	if (SDL_Init(SDL_INIT_VIDEO) != 0)
		sdl2_fail("SDL_Init: ");

	SDL_Window * window = SDL_CreateWindow("Graphics course practice 1",
		SDL_WINDOWPOS_CENTERED,
		SDL_WINDOWPOS_CENTERED,
		800, 600,
		SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_MAXIMIZED);

	if (!window)
		sdl2_fail("SDL_CreateWindow: ");

	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

	SDL_GLContext gl_context = SDL_GL_CreateContext(window);
	if (!gl_context)
		sdl2_fail("SDL_GL_CreateContext: ");

	if (auto result = glewInit(); result != GLEW_NO_ERROR)
		glew_fail("glewInit: ", result);

	if (!GLEW_VERSION_3_3)
		throw std::runtime_error("OpenGL 3.3 is not supported");

    GLuint fr_shader = create_shader(GL_FRAGMENT_SHADER, fragment_source);
    GLuint ver_shader = create_shader(GL_VERTEX_SHADER, vertex_source);
    GLuint program = create_program(ver_shader, fr_shader);

    GLuint vertex_array;
    glGenVertexArrays(1, &vertex_array);


    glClearColor(0.8f, 0.8f, 1.f, 0.f);

	bool running = true;
	while (running)
	{
		for (SDL_Event event; SDL_PollEvent(&event);) switch (event.type)
		{
		case SDL_QUIT:
			running = false;
			break;
		}

		if (!running)
			break;

		glClear(GL_COLOR_BUFFER_BIT);

        glUseProgram(program);
        glBindVertexArray(vertex_array);
        glDrawArrays(GL_TRIANGLES, 0, 3);

		SDL_GL_SwapWindow(window);
	}

	SDL_GL_DeleteContext(gl_context);
	SDL_DestroyWindow(window);
}
catch (std::exception const & e)
{
	std::cerr << e.what() << std::endl;
	return EXIT_FAILURE;
}