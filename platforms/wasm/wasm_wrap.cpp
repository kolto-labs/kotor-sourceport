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

extern "C" void* k2p_real(const char* pszName)
{
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
