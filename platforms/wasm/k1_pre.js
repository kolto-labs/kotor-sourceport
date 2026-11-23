// Runs before main() (wasm/CMakeLists.txt --pre-js). The game's data is the player's own copy, never
// part of the build: the page mounts it into the Emscripten file system and names the folder here.
//
//   Module.k2Mount(FS)    optional: mount the player's folders (e.g. FS.mkdir + FS.mount)
//   Module.k2AssetDir     the folder with chitin.key, as mounted (default /game) -> K2_ASSET_DIR
//   Module.k2UserDir      saves/ and swkotor.ini (default /user)                 -> K2_USER_DIR
//   Module.k2Env          any other variables main.cpp reads (K1_WINDOW, K2_OBB_DIR, ...)
Module['preRun'] = [].concat(Module['preRun'] || [], function () {
  if (typeof Module['k2Mount'] === 'function') Module['k2Mount'](FS);
  ENV['K2_ASSET_DIR'] = Module['k2AssetDir'] || '/game';
  ENV['K2_USER_DIR'] = Module['k2UserDir'] || '/user';
  var extra = Module['k2Env'] || {};
  for (var k in extra) ENV[k] = String(extra[k]);
});
