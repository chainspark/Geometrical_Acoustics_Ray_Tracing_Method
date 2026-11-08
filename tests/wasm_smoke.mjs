// Smoke test for the WebAssembly build: node tests/wasm_smoke.mjs
// Traces the Case II scenario in two halves and checks the result against the
// image-source solution and against a single full-range trace.
import createGart from '../web/wasm/gart.js';

const spl = (re, im) => 20 * Math.log10(Math.hypot(re, im) / 2e-5);
const fail = (msg) => { console.error('FAIL:', msg); process.exit(1); };

const gart = await createGart();
const make = () => {
  const sim = new gart.Simulator();
  sim.setRoom(10, 10, 4);
  for (let w = 0; w < 6; w++) sim.setWall(w, w === 0 ? 1 : 0, 0);
  sim.setSource(5, 5, 1, 100, 250);
  sim.setSettings(2e6, 1, 1, 0);
  sim.setGrid(2, 2.0, 40, 40, 0.1);
  sim.setLine(5.1, 5, 2, 8, 5, 2, 30, 0.1);
  const err = sim.validate();
  if (err) fail(err);
  return sim;
};

const a = make();
const t0 = performance.now();
a.trace(0, 1e6);
a.trace(1e6, 2e6);
const ms = performance.now() - t0;

const b = make();
b.trace(0, 2e6);

const pa = a.linePressure(), pb = b.linePressure(), ref = a.analyticalLine();
let sum2 = 0, n = 0;
for (let i = 0; i < a.lineSize(); i++) {
  if (Math.abs(pa[2 * i] - pb[2 * i]) > 1e-9 * (1 + Math.abs(pa[2 * i]))) fail(`split trace differs at ${i}`);
  const e = spl(pa[2 * i], pa[2 * i + 1]) - spl(ref[2 * i], ref[2 * i + 1]);
  sum2 += e * e; n++;
}
const rms = Math.sqrt(sum2 / n);
const gridHits = a.gridHits().reduce((s, h) => s + h, 0);
const paths = a.rayPaths(10);
console.log(`traced 2e6 rays in ${ms.toFixed(0)} ms, line RMS error ${rms.toFixed(3)} dB, grid hits ${gridHits}, path floats ${paths.length}`);
if (!(rms < 1.0)) fail(`RMS error too large: ${rms}`);
if (gridHits < 1000) fail('grid received too few hits');
a.delete(); b.delete();
console.log('OK');
