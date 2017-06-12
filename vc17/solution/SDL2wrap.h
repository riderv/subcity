#pragma once

namespace sdl {

struct Error 
{
    const char *msg;
    Error(const char* aMsg): msg(aMsg){}
};

struct SDL {
    bool inited = false;
    SDL() {}
    void Init()
    {
        if(inited)
            throw Error("SDL already inited.");
        if(SDL_Init(SDL_INIT_EVERYTHING) < 0)
            throw Error(SDL_GetError());
    }
    ~SDL(){
        if(inited){
            SDL_Quit();
        }
    }
};

struct Window {

    void Create(const char* title, int x, int y, int w, int h, Uint32 flags)
    {
        if(this->window)
            throw Error("SDL_Window not nullptr.");
        window = SDL_CreateWindow(title, x, y, w, h, flags);
    }
    void Destroy()
    {
        if(window) {
            SDL_DestroyWindow(window);
            window = nullptr;
        }    
    }
    ~Window() {
        Destroy();
    }
    union {
        SDL_Window *window = nullptr;
        struct Impl {
            SDL_Window *window;
            operator SDL_Window*() { return window; }
        }impl;
        
        struct Property_Width {
            SDL_Window *window;
            operator int() const {
                int h, w;
                SDL_GetWindowSize(window, &w, &h);
                return w;
            }
        }W;

        struct Property_Heigth {
            SDL_Window *window;
            operator int() const {
                int h, w;
                SDL_GetWindowSize(window, &w, &h);
                return h;
            }
        }H;

    };
};

struct Surface {
    SDL_Surface *surface = nullptr;
    ~Surface()
    {
        if(surface) {
            SDL_FreeSurface(surface);
            surface = nullptr;
        }
    }
};

struct GLContext
{
    SDL_GLContext glContext = nullptr;
    void Create(Window& window)
    {
        if(glContext)
            throw Error("SDL_GLContext alrady created.");
        this->glContext = SDL_GL_CreateContext(window.impl);
        auto er = SDL_GetError();
        if(er[0])
            throw Error(er);
    }
};


}