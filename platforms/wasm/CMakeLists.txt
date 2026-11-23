# Browser platform layer and the k1 executable (preset k1-wasm-pc: Emscripten, wasm64).
#
#   source <emsdk>/emsdk_env.sh
#   cmake --preset k1-wasm-pc && cmake --build --preset k1-wasm-pc   # -> out/k1-wasm-pc/wasm/k1.{js,wasm}
#
# The Linux build (linux/CMakeLists.txt) on Emscripten: the same engine library, shim and backends and
# the same desktop entry point (macos/main.cpp, K1's SDL_main behind it), with Android's GL stack --
# gl4es (wasm/deps.cmake, android/gl4es_glue.cpp, android/glu_es.cpp) over the WebGL context SDL makes
# -- since K1 draws in GL 1.x (immediate mode, matrix stacks, client arrays, ARB programs), which WebGL
# does not have. The root CMakeLists puts every target on wasm64 + pthreads + the SDL2 / FreeType /
# zlib ports (K2P_WASM_FLAGS) and defines K2P_MOBILE_HOST (one canvas, gl4es).
#
# The frame loop: K1's SDL_main never returns, so main() runs on a worker (-sPROXY_TO_PTHREAD), where
# a blocking loop is allowed, and draws there through an OffscreenCanvas (-sOFFSCREENCANVAS_SUPPORT,
# the page's #canvas transferred to that thread). The engine is unchanged; ASYNCIFY would also leave it
# unchanged (a yield in k1sdl.cpp's SDL_GL_SwapWindow) but instruments every function that can reach
# the swap -- through the engine's indirect calls, nearly all of them -- in code size and speed.
# The page must be cross-origin isolated (COOP/COEP headers) for SharedArrayBuffer.
#
# Game data is never bundled: main.cpp reads K2_ASSET_DIR (and K2_USER_DIR), which wasm/k1_pre.js
# fills from Module.k2AssetDir / Module.k2UserDir after the page's Module.k2Mount(FS) has mounted the
# player's folder there.

# ---- platform layer: the engine's imports that we answer ourselves ------------------------------
file(GLOB _k2_shim_sources CONFIGURE_DEPENDS ${K2P_DIR}/shim/*.cpp)   # the Win32/ASL/WGL/ASLgl port layer
add_library(k2_platform STATIC
  ${K2P_DIR}/compat/android_compat.cpp
  ${_k2_shim_sources}
  ${K2P_DIR}/null/bink_null.cpp
  ${K2P_DIR}/compat/bionic_compat.c
  ${K2P_DIR}/compat/placement_new.cpp
  ${K2P_DIR}/compat/engine_gaps.cpp
  ${K2P_DIR}/compat/engine_fix_log.cpp
  ${K2P_DIR}/compat/k2cov.cpp
  ${K2P_CONSOLE_SOURCE}
  ${K2P_QOL_SOURCE}
  ${K2P_EMBED_SOURCE}
  ${K2P_K1_PLATFORM_SOURCE}
  ${K2P_DIR}/android/gl4es_glue.cpp
  ${K2P_DIR}/android/glu_es.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/wasm_wrap.cpp)
target_include_directories(k2_platform PRIVATE "${K2P_DIR}/compat/include" "${K2P_DIR}/shim" ${K2_INCLUDE_DIRS})
# The shim includes engine headers (WinThreadsMP.h, ...): same host dialect as k2_engine.
set_source_files_properties(${_k2_shim_sources} ${K2P_QOL_SOURCE} PROPERTIES COMPILE_OPTIONS "-fno-exceptions;-fsigned-char")
set_source_files_properties(${K2P_CONSOLE_SOURCE} PROPERTIES
  COMPILE_OPTIONS "${K2_HOST_FLAGS}" COMPILE_DEFINITIONS "${K2_ENGINE_DEFS}")
# The shim's own uses of the engine headers (MacFiles.h ...) see musl's FILE tag (the same as glibc's).
target_compile_definitions(k2_platform PRIVATE K2_HOST_BUILD=1 __sFILE=_IO_FILE)
target_link_libraries(k2_platform PUBLIC k2movie k2_audio k2gl4es)
set_target_properties(k2_platform PROPERTIES CXX_STANDARD 11)
if(K2P_K1_PLATFORM_SOURCE)
  k1_platform_sources(k2_platform)   # k1/deps.cmake
endif()
if(TARGET k2_qol_assets)
  add_dependencies(k2_platform k2_qol_assets)
endif()
target_compile_definitions(k2movie PRIVATE K2M_GL_GETPROC=gl4es_GetProcAddress)   # as on Android

add_executable(${K2P_EXE} ${K2P_DIR}/macos/main.cpp ${K2P_DIR}/macos/gamedirs.cpp)
target_compile_definitions(${K2P_EXE} PRIVATE K2P_K1=1)   # main.cpp: enter K1's SDL_main
set_target_properties(${K2P_EXE} PROPERTIES CXX_STANDARD 11 RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
                                            LINK_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/k1_pre.js")
target_link_libraries(${K2P_EXE} PRIVATE
  "-Wl,--whole-archive" k2_engine "-Wl,--no-whole-archive"   # the whole engine, static ctors included
  k2_platform
  "-Wl,--allow-multiple-definition")                          # platform-side duplicates (linux/)
# The names the shim and k1sdl.cpp interpose (shim/k2wrap.h): wrapped, since a static link has no RTLD_NEXT.
set(K2P_WASM_WRAP SDL_CreateWindow SDL_GL_CreateContext SDL_GL_SetAttribute SDL_GL_SwapWindow SDL_GetDisplayMode
                  SDL_JoystickOpen SDL_PollEvent fopen stat access remove rename unlink chmod mkdir rmdir chdir
                  )   # not opendir/readdir/closedir: shim_path defines those only off K2P_MOBILE_HOST
list(TRANSFORM K2P_WASM_WRAP PREPEND "-Wl,--wrap=")
target_link_options(${K2P_EXE} PRIVATE
  ${K2P_WASM_WRAP}
  # main() on a worker, drawing through the page's #canvas transferred there (above)
  -sPROXY_TO_PTHREAD=1
  -sOFFSCREENCANVAS_SUPPORT=1
  # GL: gl4es's GLES 2 calls, client-side arrays included, looked up through SDL_GL_GetProcAddress
  -sFULL_ES2=1
  -sGL_ENABLE_GET_PROC_ADDRESS=1
  # memory: the engine sizes its own pools; a decompile's frames can be large
  -sALLOW_MEMORY_GROWTH=1
  -sINITIAL_MEMORY=512MB
  -sMAXIMUM_MEMORY=16GB
  -sSTACK_SIZE=8MB
  -sDEFAULT_PTHREAD_STACK_SIZE=2MB
  # files: the player's folder, mounted by the page (k1_pre.js); nothing is preloaded
  -sFORCE_FILESYSTEM=1
  "-sEXPORTED_RUNTIME_METHODS=['FS','ENV']"
  "--pre-js=${CMAKE_CURRENT_SOURCE_DIR}/k1_pre.js")
