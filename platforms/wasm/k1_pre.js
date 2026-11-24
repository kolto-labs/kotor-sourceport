// Runs before main() (wasm/CMakeLists.txt --pre-js). The game's data is the player's own copy, never
// part of the build: the page mounts it into the Emscripten file system and names the folder here.
//
//   Module.k2Mount(FS)    optional: mount the player's folders (e.g. FS.mkdir + FS.mount)
//   Module.k2AssetDir     the folder with chitin.key, as mounted (default /game) -> K2_ASSET_DIR
//   Module.k2UserDir      saves/ and swkotor.ini (default /user)                 -> K2_USER_DIR
//   Module.k2Env          any other variables main.cpp reads (K1_WINDOW, K2_OBB_DIR, ...)
//   Module.k2Display      optional: the canvas frames are shown on (default: one made over #canvas)
//
// Frames come from the engine's worker as ImageBitmaps (k2_present.js), since #canvas, transferred to
// that worker, cannot show them while the engine's loop runs. They are shown on a second canvas laid
// over #canvas, which lets pointer events through to #canvas, where SDL listens.
if (typeof window !== 'undefined' && !Module['k2Frame']) {
  Module['k2Frame'] = function (frame, inflight) {
    var view = Module['k2Display'];
    if (!view) {
      var under = Module['canvas'];
      view = Module['k2Display'] = document.createElement('canvas');
      view.className = 'k2-display';
      view.style.cssText = 'position:absolute;pointer-events:none;';
      under.insertAdjacentElement('afterend', view);
      var place = function () {
        view.style.left = under.offsetLeft + 'px';
        view.style.top = under.offsetTop + 'px';
        view.style.width = under.offsetWidth + 'px';
        view.style.height = under.offsetHeight + 'px';
      };
      place();
      new ResizeObserver(place).observe(under);
      window.addEventListener('resize', place);
    }
    // The frame is always released and counted off, even when showing it fails, so a failed frame
    // never stops the next ones (k2_present.js sends at most two at once).
    try {
      var ctx = view._k2ctx || (view._k2ctx = view.getContext('bitmaprenderer'));
      if (!ctx) throw new Error('this browser has no bitmaprenderer canvas');
      if (view.width !== frame.width) view.width = frame.width;
      if (view.height !== frame.height) view.height = frame.height;
      ctx.transferFromImageBitmap(frame);
    } catch (e) {
      frame.close();
      if (!view._k2warned) { view._k2warned = true; err('k1: a frame could not be shown: ' + e); }
    } finally {
      Atomics.sub(inflight, 0, 1);
    }
  };
}
Module['preRun'] = [].concat(Module['preRun'] || [], function () {
  if (typeof Module['k2Mount'] === 'function') Module['k2Mount'](FS);
  ENV['K2_ASSET_DIR'] = Module['k2AssetDir'] || '/game';
  ENV['K2_USER_DIR'] = Module['k2UserDir'] || '/user';
  var extra = Module['k2Env'] || {};
  for (var k in extra) ENV[k] = String(extra[k]);
});
