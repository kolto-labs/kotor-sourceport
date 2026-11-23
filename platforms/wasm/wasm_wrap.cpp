// The originals behind the interposed names (shim/k2wrap.h): __real_<name> for each -Wl,--wrap=<name>
// in wasm/CMakeLists.txt (K2P_WASM_WRAP), looked up by name where the other targets call dlsym.
#include <SDL.h>
#include <dirent.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define K2P_REALS(X)                                                                          \
    X(SDL_CreateWindow) X(SDL_GL_CreateContext) X(SDL_GL_SetAttribute) X(SDL_GL_SwapWindow)    \
    X(SDL_GetDisplayMode) X(SDL_JoystickOpen) X(SDL_PollEvent)                                \
    X(fopen) X(stat) X(access) X(remove) X(rename) X(unlink) X(chmod) X(mkdir) X(rmdir)        \
    X(chdir)
// Looked up by name but not wrapped: shim_path defines these only off K2P_MOBILE_HOST, so on wasm the
// shim's own directory listing calls libc directly.
#define K2P_UNWRAPPED(X) X(opendir) X(readdir) X(closedir)

#define K2P_DECLARE(n) extern "C" decltype(::n) __real_##n;
K2P_REALS(K2P_DECLARE)

// SDL's GL context path does not work on this build's engine thread (wasm_webgl.cpp): behind these
// interposed names stand the web versions, not SDL's.
extern "C" int k2p_webgl_set_attribute(SDL_GLattr attr, int value);
extern "C" SDL_GLContext k2p_webgl_create_context(SDL_Window* pWindow);
extern "C" void k2p_webgl_swap_window(SDL_Window* pWindow);

extern "C" void* k2p_real(const char* pszName)
{
    static const struct {
        const char* name;
        void* fn;
    } s_web[] = {
        {"SDL_GL_SetAttribute", reinterpret_cast<void*>(&k2p_webgl_set_attribute)},
        {"SDL_GL_CreateContext", reinterpret_cast<void*>(&k2p_webgl_create_context)},
        {"SDL_GL_SwapWindow", reinterpret_cast<void*>(&k2p_webgl_swap_window)},
    };
    for (const auto& w : s_web)
        if (strcmp(w.name, pszName) == 0) return w.fn;
#define K2P_ENTRY(n) {#n, reinterpret_cast<void*>(&__real_##n)},
#define K2P_LIBC(n) {#n, reinterpret_cast<void*>(&::n)},
    static const struct {
        const char* name;
        void* fn;
    } s_reals[] = {K2P_REALS(K2P_ENTRY) K2P_UNWRAPPED(K2P_LIBC)};
    for (const auto& r : s_reals)
        if (strcmp(r.name, pszName) == 0) return r.fn;
    return 0;
}

// KOTOR 1's SDL_main asks SDL_Init for every subsystem and stops when any is missing. Emscripten's
// SDL2 has no force feedback (SDL_INIT_HAPTIC), so the browser build asks for the rest; the engine's
// later haptic calls find no device, as on a desktop without a rumble pad.
extern "C" int __real_SDL_Init(Uint32 flags);
extern "C" int __wrap_SDL_Init(Uint32 flags)
{
    return __real_SDL_Init(flags & ~SDL_INIT_HAPTIC);
}
