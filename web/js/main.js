import { Engine } from './engine.js';
import { Heatmap } from './heatmap.js';
import { LineChart } from './linechart.js';
import { spl } from './plot-utils.js';
import {
  PRESETS, DEFAULT_PRESET, WALL_NAMES, check, clone, formatCount, getPath, raysFromExp,
  setPath, sliceAxes, toScene,
} from './scene.js';

const $ = (id) => document.getElementById(id);
const form = $('controls');
const ui = {
  preset: $('preset'), run: $('run'), stop: $('stop'), workers: $('workers'),
  raysOut: $('raysOut'), pathsOut: $('pathsOut'),
  state: $('stState'), rays: $('stRays'), rate: $('stRate'), time: $('stTime'),
  err: $('stErr'), images: $('stImages'), bar: $('stBar'),
};

const engine = new Engine();
let view = null; // RoomView, loaded lazily so a WebGL/CDN failure doesn't break the rest
const heatmap = new Heatmap($('heatmap'), $('heatTip'));
const chart = new LineChart($('linechart'), $('lineTip'));
heatmap.onImage = (img) => view?.setSliceImage(img);

let state = clone(PRESETS[DEFAULT_PRESET].state);
let mapMode = 'rt';
let current = null; // { scene, ref, sums, done, t0 }

// ---------------------------------------------------------------- form

function buildWallControls() {
  const host = $('walls');
  host.innerHTML = '<span class="hdr"></span><span class="hdr">|R|</span><span class="hdr">phase °</span>';
  WALL_NAMES.forEach((name, i) => {
    host.insertAdjacentHTML('beforeend', `
      <span class="name">${name}</span>
      <span class="mag"><input type="range" name="walls.${i}.mag" min="0" max="1" step="0.01" aria-label="${name} |R|"><output></output></span>
      <input type="number" name="walls.${i}.phaseDeg" step="5" min="-180" max="180" aria-label="${name} phase">`);
  });
}

function writeForm() {
  for (const el of form.querySelectorAll('[name]')) {
    const v = getPath(state, el.name);
    if (v === undefined) continue;
    if (el.type === 'checkbox') el.checked = !!v;
    else el.value = v;
  }
  updateOutputs();
}

function readField(el) {
  if (el.name === 'workers') return;
  const value = el.type === 'checkbox' ? el.checked : el.tagName === 'SELECT' ? Number(el.value) : el.valueAsNumber;
  if (el.type !== 'checkbox' && !Number.isFinite(value)) return;
  setPath(state, el.name, value);
}

function updateOutputs() {
  ui.raysOut.textContent = formatCount(raysFromExp(state.raysExp));
  ui.pathsOut.textContent = state.paths;
  for (const out of form.querySelectorAll('.walls .mag output')) {
    out.textContent = (+out.previousElementSibling.value).toFixed(2);
  }
}

// ---------------------------------------------------------------- results

function lineDistances(scene) {
  const l = scene.line;
  const len = Math.hypot(l.x1 - l.x0, l.y1 - l.y0, l.z1 - l.z0);
  return Float64Array.from({ length: l.n }, (_, i) => (l.n > 1 ? (len * i) / (l.n - 1) : 0));
}

const toSpl = (c) => {
  const out = new Float64Array(c.length / 2);
  for (let i = 0; i < out.length; i++) out[i] = spl(c[2 * i], c[2 * i + 1]);
  return out;
};

function redraw() {
  if (!current) return;
  const { scene, ref, sums } = current;
  const rtGrid = sums && current.done > 0 ? toSpl(sums.grid) : null;
  const anGrid = ref?.gridSpl ?? null;
  const { u, v } = sliceAxes(scene.slice.axis);
  const dims = [scene.room.L, scene.room.W, scene.room.H];
  const src = [scene.source.x, scene.source.y, scene.source.z];

  let values = null, mode = 'spl';
  if (mapMode === 'rt') values = rtGrid;
  else if (mapMode === 'an') values = anGrid;
  else if (rtGrid && anGrid) { values = rtGrid.map((x, i) => x - anGrid[i]); mode = 'diff'; }
  if (values) {
    heatmap.set({
      nu: scene.slice.nu, nv: scene.slice.nv, uAxis: u, vAxis: v, uSize: dims[u], vSize: dims[v],
      values, mode, hits: sums?.gridHits, marker: [src[u], src[v]],
    });
  }

  const rtLine = sums && current.done > 0 ? toSpl(sums.line) : null;
  chart.set({ dist: lineDistances(scene), rt: rtLine, an: ref?.lineSpl ?? null, hits: sums?.lineHits });
  ui.err.textContent = rtLine && ref ? lineRms(rtLine, ref.lineSpl).toFixed(2) : '–';
}

/** RMS dB error along the line, skipping deep nulls (> 15 dB below the line's peak). */
function lineRms(rt, an) {
  let peak = -Infinity;
  for (const a of an) if (isFinite(a)) peak = Math.max(peak, a);
  let s = 0, n = 0;
  for (let i = 0; i < rt.length; i++) {
    if (!isFinite(rt[i]) || !isFinite(an[i]) || an[i] < peak - 15) continue;
    s += (rt[i] - an[i]) ** 2; n++;
  }
  return n ? Math.sqrt(s / n) : NaN;
}

let frameQueued = false;
function scheduleRedraw() {
  if (frameQueued) return;
  frameQueued = true;
  requestAnimationFrame(() => { frameQueued = false; redraw(); updateStatus(); });
}

function updateStatus(text) {
  if (text) ui.state.textContent = text;
  if (!current) return;
  const total = current.scene.rays;
  const elapsed = (performance.now() - current.t0) / 1000;
  ui.rays.textContent = `${formatCount(current.done)} / ${formatCount(total)}`;
  ui.rate.textContent = current.done && elapsed > 0 ? (current.done / elapsed / 1e6).toFixed(1) : '–';
  ui.time.textContent = elapsed.toFixed(1);
  ui.bar.style.width = `${(100 * current.done) / total}%`;
}

// ---------------------------------------------------------------- running

async function run() {
  const issues = check(state);
  if (issues.length) {
    engine.cancel();
    updateStatus(issues[0]);
    return;
  }
  const scene = toScene(state);
  view?.setScene(scene);
  current = { scene, ref: null, sums: null, done: 0, t0: performance.now() };
  const mine = current;
  ui.run.disabled = false;
  ui.stop.disabled = false;
  updateStatus('Tracing…');
  try {
    const result = await engine.run(scene, {
      paths: state.paths,
      onReference: (r) => {
        mine.ref = { gridSpl: toSpl(r.grid), lineSpl: toSpl(r.line) };
        ui.images.textContent = r.images.toLocaleString();
        view?.setPaths(r.paths, scene.maxOrder);
        scheduleRedraw();
      },
      onProgress: (sums, done) => {
        mine.sums = sums;
        mine.done = done;
        scheduleRedraw();
      },
    });
    if (mine !== current) return;
    ui.stop.disabled = true;
    redraw();
    updateStatus(result === 'done' ? 'Done' : 'Stopped');
  } catch (err) {
    if (mine !== current) return;
    ui.stop.disabled = true;
    updateStatus(`Error: ${err.message}`);
  }
}

let debounce = 0;
function scheduleRun() {
  clearTimeout(debounce);
  debounce = setTimeout(run, 300);
}

// ---------------------------------------------------------------- wiring

function onFieldChange(e) {
  const el = e.target;
  if (!el.name) return;
  if (el.name === 'workers') {
    engine.setWorkerCount(+el.value).then(() => state.autoRun && scheduleRun());
    return;
  }
  readField(el);
  updateOutputs();
  ui.preset.value = '';
  // Instant geometry feedback, then a (debounced) re-trace.
  if (!check(state).length) view?.setScene(toScene(state));
  if (state.autoRun && el.name !== 'autoRun') scheduleRun();
}

function init() {
  buildWallControls();
  for (const [key, p] of Object.entries(PRESETS)) ui.preset.add(new Option(p.label, key));
  ui.preset.add(new Option('Custom', ''));
  ui.preset.value = DEFAULT_PRESET;

  const cores = navigator.hardwareConcurrency || 4;
  const defaultWorkers = Math.max(1, Math.min(16, cores - 1));
  for (let i = 1; i <= Math.max(cores, 1); i++) ui.workers.add(new Option(String(i), String(i)));
  ui.workers.value = String(defaultWorkers);

  writeForm();
  form.addEventListener('input', onFieldChange);
  ui.preset.addEventListener('change', () => {
    if (!ui.preset.value) return;
    state = { ...clone(PRESETS[ui.preset.value].state), autoRun: state.autoRun };
    writeForm();
    run();
  });
  ui.run.addEventListener('click', run);
  ui.stop.addEventListener('click', () => {
    engine.cancel();
    ui.stop.disabled = true;
    updateStatus('Stopped');
  });
  for (const b of document.querySelectorAll('[data-map]')) {
    b.addEventListener('click', () => {
      mapMode = b.dataset.map;
      for (const o of document.querySelectorAll('[data-map]')) o.classList.toggle('on', o === b);
      redraw();
    });
  }
  window.matchMedia('(prefers-color-scheme: dark)').addEventListener('change', () => {
    if (current) { view?.setScene(current.scene); redraw(); }
  });

  import('./view3d.js')
    .then(({ RoomView }) => {
      view = new RoomView($('view3d'));
      view.setScene(toScene(state));
      if (heatmap.data) view.setSliceImage(heatmap.image);
    })
    .catch((err) => {
      $('view3d').textContent = `3D view unavailable (${err.message}).`;
    });

  engine.setWorkerCount(defaultWorkers)
    .then(run)
    .catch((err) => updateStatus(`Could not start WebAssembly workers: ${err.message}`));
}

init();
