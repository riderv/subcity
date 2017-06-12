// solution.cpp : Defines the entry point for the application.
//

#include "stdafx.h"
#include "solution.h"
#include "SDL2wrap.h"



sdl::SDL SDL;
sdl::Window  Window;
sdl::GLContext glContext;

struct Local {
    std::function<void()> cb;
    ~Local() { cb(); }
};

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

std::vector<unsigned char> LoadPng(const char* filename)
{
    std::vector<unsigned char> image; //the raw pixels
    unsigned width, height;

    //decode
    unsigned error = lodepng::decode(image, width, height, filename);

    //if there's an error, display it
    if(error){
        string s("LoadPng fail: "); 
        s += string(lodepng_error_text(error));
        throw GLErrorStr(s);
    }
    //the pixels are now in the vector "image", 4 bytes per pixel, ordered RGBARGBA..., use it as texture, draw it, ...
    return image;
}

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
    struct TextField
    {
        GLuint outTexture = 0;
        GLuint fontTexture2Darray = 0;
        struct Glyph{
            int w=0, h=0;
            int wCount=0, hCount=0;
            int TotalCount = 0;
        }glyph;
        struct FontAtlas
        {
            string fileName;
            int w=0, h=0;
        }fontAtlas;
        int pxOffX = 0, pxOffY = 0;
        int pxXX = 0, pxYY = 0;

    }text_field;
    
    void SetText(int wCount, int hCount, const string& txt) // 
    {
        this->text_field.glyph.wCount = wCount;
        this->text_field.glyph.hCount = hCount;

    }
    void InitTextField()
    {
        // init basic data. TODO: later must be filed from external client (ie config)
        text_field.glyph.w = 8;
        text_field.glyph.h = 16;
        text_field.glyph.hCount = 30;
        text_field.glyph.wCount = 60;
        text_field.glyph.TotalCount =  text_field.glyph.hCount * text_field.glyph.wCount;
        text_field.pxXX = text_field.glyph.wCount * text_field.glyph.w;
        text_field.pxYY = text_field.glyph.hCount * text_field.glyph.h;

        // loading texture atlas for font
        text_field.fontAtlas.fileName = "res/fixf_128x256_8x16.png";
        vector<GLubyte> font_png = LoadPng(text_field.fontAtlas.fileName.c_str());
        text_field.fontAtlas.w = 128;
        text_field.fontAtlas.h = 256;

        GLuint tex_font_atlas = 0;
        Local local = {   [&tex_font_atlas]() { glDeleteTextures(1, &tex_font_atlas); tex_font_atlas = 0; }   };
        
        glCreateTextures(GL_TEXTURE_2D, 1, &tex_font_atlas);
        glTextureStorage2D(tex_font_atlas, 1, GL_RG8UI, 128, 256);
        glTextureSubImage2D(tex_font_atlas, 0, 0, 0, 128, 256, GL_RGBA, GL_UNSIGNED_BYTE, font_png.data() );
         
         
        // 2d array of 8x16 texture filed from font atlas
        gl::glCreateTextures(GL_TEXTURE_2D_ARRAY, 1, &text_field.fontTexture2Darray);
        gl::glTextureStorage3D(text_field.fontTexture2Darray, 1, GL_R8UI, text_field.glyph.w, text_field.glyph.h, text_field.glyph.TotalCount);

        const int one_letter_pixels_sz = text_field.glyph.w * text_field.glyph.h;
        vector<GLbyte> one_letter_pixels(one_letter_pixels_sz);
        
        // Copy subimages of letters from atlas to tex-ar-2d
        int i = 0;
        for( int yoff=0; yoff < text_field.glyph.h; yoff++ )
        {
            for(int xoff=0; xoff < text_field.glyph.w; xoff++)
            {            
                glGetTextureSubImage(tex_font_atlas, 0, xoff, yoff, 0,
                    text_field.glyph.w,
                    text_field.glyph.h,
                    1, GL_R8UI, GL_UNSIGNED_BYTE, one_letter_pixels_sz, &one_letter_pixels[0]);

                glTextureSubImage3D(text_field.fontTexture2Darray, 
                    0, //miplevel
                    0,0, (i++), //x,y,z offsets
                    text_field.glyph.w,
                    text_field.glyph.h,                    
                    1, //depth
                    GL_R8UI, GL_UNSIGNED_BYTE,
                    one_letter_pixels.data());
            }
        }


    }
    
    
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
    // TRINGLE STRIP      x    y     z     w
    const vec4[4] q = { {0.0, 0.0,  0.0,  1.0}, 
                        {0.0, 1.0,  0.0,  1.0}, 
                        {1.0, 0.0,  0.0,  1.0},
                        {1.0, 1.0,  0.0,  1.0} };
        vec4 v = q[gl_VertexID];
        v.x *= 0.125;
        v.y *= 0.125;
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

        InitTextField();

        isRun = true;
    }
    void Run()
    {
        glUseProgram(prog);
        glClear(GL_COLOR_BUFFER_BIT);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
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
