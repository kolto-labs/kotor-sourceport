# Third-party code the browser build compiles itself. SDL2, FreeType and zlib are Emscripten ports
# (-sUSE_SDL=2 -sUSE_FREETYPE=1 -sUSE_ZLIB=1, wasm/CMakeLists.txt); what is left is gl4es, the GL
# 1.x/ARB surface the shim and K1's GL layer target, here over the WebGL (GLES 2) context SDL makes,
# with the same pinned source and patches as Android and iOS (android/gl4es_patch.cmake). Fetched once
# into platforms/out/_deps-wasm.
#
# The ports' compile and link flags are on every target already (K2P_WASM_FLAGS, root CMakeLists);
# SDL2::SDL2 stands for the port so that video/CMakeLists.txt builds the movie player against it.
add_library(SDL2::SDL2 INTERFACE IMPORTED GLOBAL)

set(FETCHCONTENT_BASE_DIR "${K2P_DIR}/out/_deps-wasm" CACHE PATH "")   # before the include
include(FetchContent)
set(FETCHCONTENT_QUIET OFF)

FetchContent_Declare(k2gl4es
  URL https://github.com/ptitSeb/gl4es/archive/a444cc94b17c672c66c3e6ce07428dc603034db1.tar.gz
  URL_HASH SHA256=54dc6e6b5eb04f9a9eb7945ba564b868dd65e7fceeabafe02eaa68cb735f04ce
  SOURCE_SUBDIR k2-no-cmake    # sources only: MakeAvailable finds no CMakeLists.txt to add
  PATCH_COMMAND ${CMAKE_COMMAND} -P "${K2P_DIR}/android/gl4es_patch.cmake")
FetchContent_MakeAvailable(k2gl4es)
# PATCH_COMMAND runs once, when the sources are fetched; the patch is idempotent, so it is applied
# on every configure too, and an edit to it reaches an existing download.
execute_process(COMMAND ${CMAKE_COMMAND} -P "${K2P_DIR}/android/gl4es_patch.cmake" WORKING_DIRECTORY "${k2gl4es_SOURCE_DIR}"
                COMMAND_ERROR_IS_FATAL ANY)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${K2P_DIR}/android/gl4es_patch.cmake")
file(GLOB _gl4es_src CONFIGURE_DEPENDS "${k2gl4es_SOURCE_DIR}/src/gl/*.c" "${k2gl4es_SOURCE_DIR}/src/gl/wrap/*.c"
                                       "${k2gl4es_SOURCE_DIR}/src/gl/math/*.c")
add_library(k2gl4es STATIC ${_gl4es_src} "${k2gl4es_SOURCE_DIR}/src/glx/hardext.c")
target_include_directories(k2gl4es PUBLIC "${k2gl4es_SOURCE_DIR}/include"
                                   PRIVATE "${k2gl4es_SOURCE_DIR}/src/glx")
target_compile_definitions(k2gl4es PRIVATE NOX11 NOEGL NO_GBM NO_LOADER NO_INIT_CONSTRUCTOR STATICLIB
                                           DEFAULT_ES=2)
target_compile_options(k2gl4es PRIVATE -std=gnu99 -O2 -w)
