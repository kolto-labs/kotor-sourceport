// GL contexts on the engine's thread (browser build).
//
// SDL2's Emscripten backend makes, binds and presents its GL contexts through EGL, and Emscripten
// runs EGL on the page's thread. Here main() runs on a worker (PROXY_TO_PTHREAD) and the page's
// #canvas has been handed to it as an OffscreenCanvas, so the page's thread can no longer make a
// context on it ("Cannot get context from a canvas that has transferred its control to offscreen").
// So this build makes the WebGL context on the engine's thread itself and answers SDL's GL context
// calls from it; windows, events and the rest of SDL stay SDL's.
//
// The engine never returns to the browser between frames (its loop blocks), so frames are presented
// explicitly: explicitSwapControl, and a commit at every swap.
//
// SDL_GL_CreateContext, SDL_GL_SwapWindow and SDL_GL_SetAttribute are interposed by k1/k1sdl.cpp;
// wasm_wrap.cpp hands it these functions as the "originals" behind them. The other context calls
// are wrapped here directly (-Wl,--wrap, wasm/CMakeLists.txt).
#include <SDL.h>
#include <emscripten/html5.h>
#include <stdint.h>

namespace {

const char* const kCanvas = "#canvas";
int s_attrs[SDL_GL_FLOATBUFFERS + 1];   // what the engine asked for, by SDL_GLattr
bool s_set[SDL_GL_FLOATBUFFERS + 1];
thread_local SDL_Window* t_window;
thread_local EMSCRIPTEN_WEBGL_CONTEXT_HANDLE t_context;

int Asked(SDL_GLattr attr, int nDefault)
{
    return attr >= 0 && attr <= SDL_GL_FLOATBUFFERS && s_set[attr] ? s_attrs[attr] : nDefault;
}

} // namespace

extern "C" int __real_SDL_GL_SetAttribute(SDL_GLattr attr, int value);

// SDL_GL_GetAttribute reads most values back from a current context, so the request is kept here.
extern "C" int k2p_webgl_set_attribute(SDL_GLattr attr, int value)
{
    if (attr >= 0 && attr <= SDL_GL_FLOATBUFFERS) {
        s_attrs[attr] = value;
        s_set[attr] = true;
    }
    return __real_SDL_GL_SetAttribute(attr, value);
}

extern "C" SDL_GLContext k2p_webgl_create_context(SDL_Window* pWindow)
{
    EmscriptenWebGLContextAttributes a;
    emscripten_webgl_init_context_attributes(&a);
    a.majorVersion = 1;   // gl4es draws in GLES 2, which is WebGL 1
    a.minorVersion = 0;
    a.alpha = Asked(SDL_GL_ALPHA_SIZE, 0) > 0;
    a.depth = Asked(SDL_GL_DEPTH_SIZE, 16) > 0;
    a.stencil = Asked(SDL_GL_STENCIL_SIZE, 0) > 0;
    a.antialias = Asked(SDL_GL_MULTISAMPLESAMPLES, 0) > 0;
    a.preserveDrawingBuffer = 0;
    a.explicitSwapControl = 1;
    a.proxyContextToMainThread = EMSCRIPTEN_WEBGL_CONTEXT_PROXY_DISALLOW;
    EMSCRIPTEN_WEBGL_CONTEXT_HANDLE ctx = emscripten_webgl_create_context(kCanvas, &a);
    if (ctx <= 0) {
        SDL_SetError("WebGL context on %s: error %d", kCanvas, static_cast<int>(ctx));
        return 0;
    }
    if (emscripten_webgl_make_context_current(ctx) != EMSCRIPTEN_RESULT_SUCCESS) {
        emscripten_webgl_destroy_context(ctx);
        SDL_SetError("WebGL context: could not make it current");
        return 0;
    }
    t_window = pWindow;
    t_context = ctx;
    return reinterpret_cast<SDL_GLContext>(static_cast<intptr_t>(ctx));
}

extern "C" void k2p_webgl_swap_window(SDL_Window*)
{
    if (t_context) emscripten_webgl_commit_frame();
}

extern "C" int __wrap_SDL_GL_MakeCurrent(SDL_Window* pWindow, SDL_GLContext context)
{
    EMSCRIPTEN_WEBGL_CONTEXT_HANDLE ctx = static_cast<EMSCRIPTEN_WEBGL_CONTEXT_HANDLE>(reinterpret_cast<intptr_t>(context));
    if (emscripten_webgl_make_context_current(ctx) != EMSCRIPTEN_RESULT_SUCCESS)
        return SDL_SetError("WebGL context: could not make it current");
    t_context = ctx;
    t_window = ctx ? pWindow : 0;
    return 0;
}

extern "C" SDL_GLContext __wrap_SDL_GL_GetCurrentContext(void)
{
    return reinterpret_cast<SDL_GLContext>(static_cast<intptr_t>(t_context));
}

extern "C" SDL_Window* __wrap_SDL_GL_GetCurrentWindow(void)
{
    return t_window;
}

extern "C" void __wrap_SDL_GL_DeleteContext(SDL_GLContext context)
{
    EMSCRIPTEN_WEBGL_CONTEXT_HANDLE ctx = static_cast<EMSCRIPTEN_WEBGL_CONTEXT_HANDLE>(reinterpret_cast<intptr_t>(context));
    if (!ctx) return;
    if (ctx == t_context) {
        emscripten_webgl_make_context_current(0);
        t_context = 0;
        t_window = 0;
    }
    emscripten_webgl_destroy_context(ctx);
}

// The browser paces frames itself; there is no swap interval to set.
extern "C" int __wrap_SDL_GL_SetSwapInterval(int)
{
    return 0;
}

extern "C" void __wrap_SDL_GL_GetDrawableSize(SDL_Window*, int* pnWidth, int* pnHeight)
{
    int w = 0, h = 0;
    emscripten_get_canvas_element_size(kCanvas, &w, &h);
    if (pnWidth) *pnWidth = w;
    if (pnHeight) *pnHeight = h;
}
