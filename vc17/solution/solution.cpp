// solution.cpp : Defines the entry point for the application.
//

#include "stdafx.h"
#include "solution.h"
#include "SDL2wrap.h"




SDL_Window  *Window = 0;
int Window_Width = -1, Window_Height = -1;
SDL_GLContext glContext = 0;


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

struct SDLError
{
    SDLError(const char* aMsg):msg(aMsg){}
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

    //fprintf(stderr, "%s\n", message);
    //if (severity==GL_DEBUG_SEVERITY_HIGH)
    {
        OutputDebugStringA("\nopenglCallbackFunction:\nseverity==GL_DEBUG_SEVERITY_HIGH\n");
        OutputDebugStringA(message);
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
    App():app(this){}
    bool isRun = false;
    GLuint vao = 0;
    GLuint prog = 0;


        GLuint      text_buffer;
        GLuint      font_texture;


        GLuint       text_program;
        vector<char> screen_buffer;
        int          buffer_width;
        int          buffer_height;


    
    void SetText(int wCount, int hCount, const string& txt) // 
    {
    
    }
    void InitTextField()
    {
        // init basic data. TODO: later must be filed from external client (ie config)

        // loading texture atlas for font
        auto fileName = "d:\\_pro\\vc17\\subcity\\res\\fixf_128x256_8x16.png";
        vector<GLubyte> font_png = LoadPng(fileName);

        GLuint tex_font_atlas = 0;
        struct Local {
            GLuint &tex_font_atlas_;
            ~Local() { glDeleteTextures(1, &tex_font_atlas_); }
        } local {tex_font_atlas};
        glCreateTextures(GL_TEXTURE_2D, 1, &tex_font_atlas);
        glTextureStorage2D(tex_font_atlas, 1, GL_R8UI, 128,256);
        glTextureParameteri(tex_font_atlas, GL_TEXTURE_MIN_FILTER,      GL_NEAREST);
        glTextureParameteri(tex_font_atlas, GL_TEXTURE_MAG_FILTER,      GL_NEAREST);
        glTextureParameteri(tex_font_atlas, GL_TEXTURE_WRAP_S,          GL_CLAMP_TO_EDGE);
        glTextureParameteri(tex_font_atlas, GL_TEXTURE_WRAP_T,          GL_CLAMP_TO_EDGE);
        
        glTextureSubImage2D(tex_font_atlas, 0, 0,0, 128,256, GL_RGBA_INTEGER, GL_UNSIGNED_BYTE,  font_png.data() );
        auto err = glGetError();
        if( err != GL_NO_ERROR )
            __debugbreak();
        


        // 2d array of 8x16 texture filed from font atlas
        gl::glCreateTextures(GL_TEXTURE_2D_ARRAY, 1, &font_texture);
        //                                 //levels
        gl::glTextureStorage3D(font_texture, 1, GL_R8UI, 8, 16, 256);
        //Always set reasonable texture parameters
        glTextureParameteri(font_texture,   GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTextureParameteri(font_texture,   GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTextureParameteri(font_texture,   GL_TEXTURE_WRAP_S,     GL_CLAMP_TO_EDGE);
        glTextureParameteri(font_texture,   GL_TEXTURE_WRAP_T,     GL_CLAMP_TO_EDGE);


        int srcX = 0, srcY = 0, srcZ = 0, i = 0;
        for(srcY = 0;  srcY < 16;  ++srcY)
        {
            for(srcX = 0; srcX < 16; ++srcX)
            {
                glCopyImageSubData(
                    tex_font_atlas,
                    GL_TEXTURE_2D,
                    0, //The mipmap level to read from the source.
                    srcX*8,
                    srcY*16,
                    srcZ,
                    font_texture, GL_TEXTURE_2D_ARRAY,
                    0, //destLevel
                    0, //dstX
                    0, //dstY
                    i, //dstZ
                    8, 16, 
                    1); // The depth of the region to be copied
                i++;
            }
        }


        // Copy subimages of letters from atlas to tex-ar-2d
        //int i = 0;
        //for( int yoff=0; yoff < (256/16); yoff++ )
        //{
        //    for(int xoff=0; xoff < (128/8); xoff++)
        //    {            
        //        //glGetTextureSubImage(local.tex_font_atlas, 0, xoff*8, yoff*16, 0,
        //        //    8,
        //        //    16,
        //        //    1, //depth
        //        //    GL_RGBA, GL_UNSIGNED_BYTE, L_sz, L);

        //        glTextureSubImage3D(font_texture, 
        //            0, //miplevel
        //            0,0, (i++), //x,y,z offsets
        //            8,
        //            16,
        //            1, //depth
        //            GL_RGBA, GL_UNSIGNED_BYTE,
        //            L);
        //    }
        //}

                             
        //
        //for( i = 0; i < 2; ++i)
        //{
        //    char *data = &L[i*8*16];
        //    glTextureSubImage3D(font_texture, 
        //        0, //level (mip)
        //        0, //xoffset
        //        0, //yoffest
        //        i, //zoffset (for 2darray it is index)
        //        8, 16, // w, h
        //        1, // depth ??
        //        GL_RED, GL_UNSIGNED_BYTE, data );
        //
        //}
   
        // texture for ascii text buffer
        glCreateTextures(GL_TEXTURE_2D, 1, &text_buffer);
        glTextureStorage2D(text_buffer, 1, GL_R8UI, 1024, 1024);
        int clear_data = 0;        //level
        glClearTexImage(text_buffer, 0, GL_RED_INTEGER, GL_UNSIGNED_BYTE, &clear_data);
        //Always set reasonable texture parameters
        glTextureParameteri(text_buffer, GL_TEXTURE_MIN_FILTER,GL_NEAREST);
        glTextureParameteri(text_buffer, GL_TEXTURE_MAG_FILTER,GL_NEAREST);
        glTextureParameteri(text_buffer, GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
        glTextureParameteri(text_buffer, GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);


        char text[256*256] = {0};
        strcpy(text, "Some text data. Какой то матье его текст!");
        glTextureSubImage2D(text_buffer, 0, 0,0,      256, 256, GL_RED_INTEGER, GL_UNSIGNED_BYTE, text );

        //vector<char> text(1024*1024);
        //for( unsigned i = 0; i < text.size(); ++i )
        //{
        //    text[i] = rand()%256;
        //    
        //}                           // levl,offx,offy, w,  h,
        //glTextureSubImage2D(text_buffer, 0, 0,0,      1024, 1024, GL_RED_INTEGER, GL_UNSIGNED_BYTE, text.data() );


    }
    
    
    void Init()
    {
        {
            using namespace glbinding;
            //setCallbackMaskExcept(CallbackMask::After, { "glGetError" });
            //setAfterCallback([](const FunctionCall &)
            //{
            //    const auto error = glGetError();
            //    if (error != GL_NO_ERROR) {
            //        __debugbreak();
            //    }
            //        
            //});
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
    const vec4[4] q = { {1.5, 0.0,   0.0,  1.0}, 
                        {1.5, 2.0,   0.0,  1.0}, 
                        {2.0, 0.0,   0.0,  1.0},
                        {2.0, 2.0,   0.0,  1.0} };
        vec4 v = q[gl_VertexID];
        v.x -= 1.0;
        v.y -= 1.0;
        gl_Position = v;
}
)";
        CompileShader(vs, vs_src);

        static const char * fs_src =
        {R"(
#version 440 core
layout (origin_upper_left) in vec4 gl_FragCoord;
layout (location = 0) out vec4 o_color;
layout (binding = 0) uniform isampler2D text_buffer;
layout (binding = 1) uniform isampler2DArray font_texture;

layout (location = 3) uniform ivec4 pxXY;
void main()
{
    ivec2 frag_coord = ivec2(gl_FragCoord.xy) - pxXY.xy;
    if(frag_coord.x > pxXY[2] )
       discard;

    ivec2 char_size = textureSize(font_texture, 0).xy;
    ivec2 char_location = frag_coord / char_size;
    ivec2 texel_coord = frag_coord % char_size;
    int character = texelFetch(text_buffer, char_location, 0).x;
    float val = texelFetch(font_texture, ivec3(texel_coord, character), 0).x;
    if (val == 0.0)
        discard;
    o_color = vec4(0.0, 0.0, 0.0, 1.0);
}
        )"};

        auto fs_src2 =
R"(
#version 440 core
out vec4 o_color;
void main()
{
    o_color = vec4(0.5, 0.7, 0.5, 1.0);
}
)";
        CompileShader(fs, fs_src);

        glAttachShader(prog, vs);
        glAttachShader(prog, fs);
        LinkProgram(prog);
        
        glDeleteShader(vs);
        glDeleteShader(fs);
        
        glUseProgram(prog);

        InitTextField();
        glActiveTexture(GL_TEXTURE0);
        glBindTextureUnit(0, text_buffer);
        
        glActiveTexture(GL_TEXTURE1);
        glBindTextureUnit(1, font_texture);
        

        
        int pxX = 1.5f*Window_Width/2;
        int pxXX = 2.0f*Window_Width/2 - 8 - pxX;
        glUniform4i(3, pxX, 0, pxXX, 0 ); 

        glClearColor(0.7f, 0.7f, 0.7f, 1.0f);

        
        isRun = true;
    }
    void Run()
    {

        glClear(GL_COLOR_BUFFER_BIT);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    }
    void OnQuit() { isRun = false; }
    union {
        App *app;
        struct Property_IsRun {
            App *app;
            operator bool() const { return app->isRun; }
        }IsRun;
    };
    //bool IsRun() const { return isRun; }
}app;

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
                     _In_opt_ HINSTANCE hPrevInstance,
                     _In_ LPWSTR    lpCmdLine,
                     _In_ int       nCmdShow)
{
    try {
        if(SDL_Init(SDL_INIT_EVERYTHING) < 0)
            throw SDLError(SDL_GetError());

        SDL_GL_SetAttribute(SDL_GL_ACCELERATED_VISUAL, 1);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 5);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_DEBUG_FLAG);

        // Also request a depth buffer
        SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
        SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
        SDL_GL_LoadLibrary(NULL);

        Window = SDL_CreateWindow("Subcity", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 800, 800,
            SDL_WINDOW_OPENGL);

        glContext = SDL_GL_CreateContext(Window);
        auto er = SDL_GetError();
        if(er[0])
            throw SDLError(er);

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
        

        SDL_Event event;
        __int64 ticks = 0;
        
        SDL_GetWindowSize(Window, &Window_Width, &Window_Height);
        glViewport(0,0, Window_Width, Window_Height);
        app.Init();

        while( app.IsRun )
        {
            while( SDL_PollEvent(&event) ) 
            {            
                if( event.type == SDL_QUIT )
                    app.OnQuit();
                if( event.type == SDL_WINDOWEVENT ) {
                    if( event.window.event == SDL_WINDOWEVENT_RESIZED )
                    {
                        glViewport(0,0, Window_Width, Window_Height); // todo view matrix need recalculation
                    }
                }
            }
            app.Run();
            SDL_GL_SwapWindow(Window);
            ticks++;
        }
        char sTickCount[1024] = {0};
        sprintf_s(sTickCount, "%I64d", ticks);
        //MessageBoxA(0, sTickCount, "Exit message loop", 0);
    }
    catch(GLErrorStr& E)
    {
        MessageBoxA(0, E.msg.c_str(), "GLErrorStr", MB_ICONERROR);
    }
    catch(GLError &E)
    {
        MessageBoxA(0, E.msg, "GLError", MB_ICONERROR);
    }
    catch(SDLError &E)
    {
        MessageBoxA(0, E.msg, "SDL ERROR", MB_ICONERROR);
    }
    catch(...)
    {
        MessageBoxA(0, "Unknown error.", "ERROR", MB_ICONWARNING);
    }
    return 0;
}
