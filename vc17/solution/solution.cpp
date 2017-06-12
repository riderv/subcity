// solution.cpp : Defines the entry point for the application.
//

#include "stdafx.h"
#include "solution.h"
#include "SDL2wrap.h"



sdl::SDL SDL;
sdl::Window  Window;
sdl::GLContext glContext;


std::stringstream& operator<<(std::stringstream& ss, std::vector<char>& vec)
{
    for(unsigned i = 0; i < vec.size(); ++i )
    {
        ss << vec[i];
    }
    return ss;
}

struct GLError
{
    GLError(const char* aMsg):msg(aMsg){}
    const char* msg;    
};
struct GLErrorStr
{
    GLErrorStr(std::string& aMsg): msg(aMsg) {}
    GLErrorStr(std::string&& aMsg): msg(aMsg) {}
    std::string msg;
};

static void APIENTRY openglCallbackFunction(
    GLenum source,
    GLenum type,
    GLuint id,
    GLenum severity,
    GLsizei length,
    const GLchar* message,
    const void* userParam
)
{
    (void)source; (void)type; (void)id; 
    (void)severity; (void)length; (void)userParam;
    OutputDebugStringA(message);

    //fprintf(stderr, "%s\n", message);
    if (severity==GL_DEBUG_SEVERITY_HIGH) {
        OutputDebugStringA("\nseverity==GL_DEBUG_SEVERITY_HIGH\n");
        __debugbreak();
        throw GLErrorStr(std::string(message));
    }
}

void CompileShader(GLuint shader, const char* shader_src)
{
    if(shader == 0)
        throw "CompileShader failed: shader id is 0";
    glShaderSource(shader, 1, &shader_src, nullptr);
    glCompileShader(shader);
    GLint status = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
    if(GL_FALSE == status)
    {
        GLint logSize = 0;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &logSize);
        std::vector<GLchar> errorLog(logSize);
        glGetShaderInfoLog(shader, logSize, &logSize, &errorLog[0]);
        std::stringstream ss;
        ss << "Error CompileShader (" << shader << ") ";
        ss << errorLog;        
        throw GLErrorStr(ss.str());
    }
}

void LinkProgram(GLuint prog)
{
    glLinkProgram(prog);
    GLint status;
    glGetProgramiv(prog, GL_LINK_STATUS, &status);
    if(GL_FALSE == status) {
        GLint logSize = 0;
        glGetProgramiv(prog, GL_INFO_LOG_LENGTH, &logSize);
        std::vector<GLchar> errorLog(logSize);
        std::stringstream ss;
        ss << "Error LinkProgram (" << prog << ") ";
        ss << errorLog;        
        throw GLErrorStr(ss.str());
    }
}

struct App
{
    bool isRun = false;
    GLuint vao = 0;
    GLuint prog = 0;
    
    void Init()
    {
        {
            using namespace glbinding;
            setCallbackMaskExcept(CallbackMask::After, { "glGetError" });
            setAfterCallback([](const FunctionCall &)
            {
                const auto error = glGetError();
                if (error != GL_NO_ERROR) {
                    __debugbreak();
                }
                    
            });
        }
        glDebugMessageCallback(openglCallbackFunction, nullptr);
        glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, NULL, true);

        glDisable(GL_DEPTH_TEST);
        glDisable(GL_CULL_FACE);

        glCreateVertexArrays(1, &vao);
        glBindVertexArray(vao);
        GLuint vs = glCreateShader(GL_VERTEX_SHADER);
        GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
        prog = glCreateProgram();

        const char *vs_src =
R"(#version 440 core
void main()
{
    // TRINGLE STRIP             x    y     z     w
    const vec4[4] q = { {0.0, 0.0,  0.0,  1.0}, 
                        {0.0, 1.0,  0.0,  1.0}, 
                        {1.0, 0.0,  0.0,  1.0},
                        {1.1, 1.1,  0.0,  1.0} };
        vec4 v = q[gl_VertexID];
        v*=0.1;
        gl_Position = v;
}
)";
        CompileShader(vs, vs_src);

        const char* fs_src = 
R"(#version 440 core
out vec4 color;
void main()
{
    color = vec4( 0.6, 0.4, 1.0, 1.0 );
}
)";
        CompileShader(fs, fs_src);

        glAttachShader(prog, vs);
        glAttachShader(prog, fs);
        LinkProgram(prog);
        
        glDeleteShader(vs);
        glDeleteShader(fs);
        glUseProgram(prog);

        
        glViewport(0,0, Window.W, Window.H);
        glClearColor(0.0, 0.0, 0.0, 1.0);

        isRun = true;
    }
    void Run()
    {
        glClear(GL_COLOR_BUFFER_BIT);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 3);
    }
    void OnQuit() { isRun = false; }
    bool IsRun() const { return isRun; }
}app;

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
                     _In_opt_ HINSTANCE hPrevInstance,
                     _In_ LPWSTR    lpCmdLine,
                     _In_ int       nCmdShow)
{
    try {
        SDL.Init();
        SDL_GL_SetAttribute(SDL_GL_ACCELERATED_VISUAL, 1);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 5);
        // Also request a depth buffer
        SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
        SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
        SDL_GL_LoadLibrary(NULL);

        Window.Create("Subcity", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 1024, 768,
            SDL_WINDOW_MAXIMIZED |
            SDL_WINDOW_OPENGL |
            SDL_WINDOW_RESIZABLE
            );

        glContext.Create(Window);
        glbinding::Binding::initialize();

        {
            std::stringstream OpenGL;
            OpenGL << "OpenGL loaded\n";
            OpenGL << "\nVendor: "  << glGetString(GL_VENDOR);
            OpenGL << "\nRendere: " << glGetString(GL_RENDERER);
            OpenGL << "\nVersion: " << glGetString(GL_VERSION);
            OutputDebugStringA(OpenGL.str().c_str());
        }
        SDL_GL_SetSwapInterval(1);
        app.Init();
        
        SDL_Event event;
        __int64 ticks = 0;
        while(app.IsRun() )
        {
            while( SDL_PollEvent(&event) ) 
            {            
                if( event.type == SDL_QUIT )
                    app.OnQuit();                
            }
            app.Run();
            SDL_GL_SwapWindow(Window.impl);
            ticks++;
        }
        char sTickCount[1024] = {0};
        sprintf_s(sTickCount, "%I64d", ticks);
        MessageBoxA(0, sTickCount, "Exit message loop", 0);
    }
    catch(GLErrorStr& E)
    {
        MessageBoxA(0, E.msg.c_str(), "GLErrorStr", MB_ICONERROR);
    }
    catch(GLError &E)
    {
        MessageBoxA(0, E.msg, "GLError", MB_ICONERROR);
    }
    catch(sdl::Error &E)
    {
        MessageBoxA(0, E.msg, "SDL error.", MB_ICONERROR);
    }
    catch(...)
    {
        MessageBoxA(0, "Unknown error.", "ERROR", MB_ICONWARNING);
    }
    return 0;
}
