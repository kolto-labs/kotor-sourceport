// Interposition on a static link (Emscripten): there is no dlsym(RTLD_NEXT) to find the definition
// behind ours, so the link wraps each interposed name (-Wl,--wrap=<name>, wasm/CMakeLists.txt): our
// definition is __wrap_<name> (K2P_WRAP), every call of <name> lands there, and the original stays
// reachable as __real_<name>, which wasm/wasm_wrap.cpp hands back by name (K2P_NEXT). Elsewhere
// both macros are the dynamic-link spelling, unchanged.
#pragma once

#ifdef __EMSCRIPTEN__
#define K2P_WRAP(n) __wrap_##n
#define K2P_WRAPPED(n) extern "C" decltype(::n) __wrap_##n;
#define K2P_NEXT(n) k2p_real(n)
extern "C" void* k2p_real(const char* pszName);   // 0 when the name is not wrapped
#else
#include <dlfcn.h>
#define K2P_WRAP(n) n
#define K2P_WRAPPED(n)
#define K2P_NEXT(n) dlsym(RTLD_NEXT, n)
#endif
