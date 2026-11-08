// Web Worker: owns one WebAssembly Simulator and traces ray ranges on request.
import createGart from '../wasm/gart.js';

const ready = createGart().then((gart) => new gart.Simulator());
let sim = null;

/** Applies a scene description (see scene.js) to the simulator. */
function configure(s) {
  sim.setRoom(s.room.L, s.room.W, s.room.H);
  s.walls.forEach((w, i) => {
    const phi = (w.phaseDeg * Math.PI) / 180;
    sim.setWall(i, w.mag * Math.cos(phi), w.mag * Math.sin(phi));
  });
  sim.setSource(s.source.x, s.source.y, s.source.z, s.source.powerDb, s.source.freq);
  sim.setSettings(s.rays, s.maxOrder, s.seed, 0);
  sim.setGrid(s.slice.axis, s.slice.offset, s.slice.nu, s.slice.nv, s.radius); // after setRoom
  const l = s.line;
  sim.setLine(l.x0, l.y0, l.z0, l.x1, l.y1, l.z1, l.n, s.radius);
  return sim.validate();
}

self.onmessage = async ({ data: msg }) => {
  sim ??= await ready;
  try {
    switch (msg.type) {
      case 'init':
        self.postMessage({ type: 'ready', id: msg.id });
        break;
      case 'configure': {
        const error = configure(msg.scene);
        self.postMessage({ type: 'configured', id: msg.id, error });
        break;
      }
      case 'trace': {
        sim.reset();
        sim.trace(msg.begin, msg.end);
        const grid = sim.gridPressure(), gridHits = sim.gridHits();
        const line = sim.linePressure(), lineHits = sim.lineHits();
        self.postMessage(
          { type: 'traced', id: msg.id, begin: msg.begin, end: msg.end, grid, gridHits, line, lineHits },
          [grid.buffer, gridHits.buffer, line.buffer, lineHits.buffer],
        );
        break;
      }
      case 'reference': {
        const grid = sim.analyticalGrid(), line = sim.analyticalLine();
        const paths = sim.rayPaths(msg.paths);
        const images = sim.imageCount();
        self.postMessage({ type: 'reference', id: msg.id, grid, line, paths, images },
          [grid.buffer, line.buffer, paths.buffer]);
        break;
      }
      default:
        throw new Error(`unknown message ${msg.type}`);
    }
  } catch (err) {
    self.postMessage({ type: 'error', id: msg.id, error: String(err?.message ?? err) });
  }
};
