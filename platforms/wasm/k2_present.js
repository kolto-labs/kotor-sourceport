// Frame presentation for the browser build (--js-library, wasm/CMakeLists.txt).
//
// The engine draws on its own worker into the page's #canvas, transferred there as an OffscreenCanvas.
// A worker's OffscreenCanvas reaches the screen only when the worker returns to its event loop, and
// K1's frame loop never returns; emscripten_webgl_commit_frame is a no-op, since browsers removed
// commit(). So each finished frame is taken out of the canvas as an ImageBitmap (no copy) and posted
// to the page, which shows it with a "bitmaprenderer" canvas (Module.k2Frame, k1_pre.js).
//
// At most two frames are on their way at once: while the page is busy the engine keeps running and
// drops frames instead of queueing them.
addToLibrary({
  $k2Present: {inflight: null},

  k2p_present_frame__deps: ['$GL', '$k2Present', '$CMD_CALL_HANDLER'],
  k2p_present_frame: () => {
    var ctx = GL.currentContext;
    var canvas = ctx?.GLctx?.canvas;
    if (!canvas?.transferToImageBitmap) return 0;
    var inflight = (k2Present.inflight ||= new Int32Array(new SharedArrayBuffer(4)));
    if (Atomics.load(inflight, 0) >= 2) return 0;
    var frame = canvas.transferToImageBitmap();
    Atomics.add(inflight, 0, 1);
    // The page side of the pthread protocol calls Module[handler](...args) for this command.
    postMessage({cmd: CMD_CALL_HANDLER, handler: 'k2Frame', args: [frame, inflight]}, [frame]);
    return 1;
  },
});
