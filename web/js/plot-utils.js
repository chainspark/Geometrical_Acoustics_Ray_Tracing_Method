// Shared helpers for the canvas plots.

export function cssVar(name) {
  return getComputedStyle(document.documentElement).getPropertyValue(name).trim();
}

/** Round tick positions covering [min, max]. */
export function niceTicks(min, max, count = 6) {
  if (!(isFinite(min) && isFinite(max)) || max <= min) return [min];
  const raw = (max - min) / count;
  const mag = 10 ** Math.floor(Math.log10(raw));
  const step = [1, 2, 2.5, 5, 10].map((m) => m * mag).find((s) => s >= raw) ?? raw;
  const ticks = [];
  for (let t = Math.ceil(min / step - 1e-9) * step; t <= max + 1e-9 * step; t += step) ticks.push(+t.toFixed(10));
  return ticks;
}

export const fmt = (v, d = 1) => (isFinite(v) ? v.toFixed(d) : '–');

/** Keeps a canvas' backing store matched to its CSS size and device pixel ratio. */
export function autoSize(canvas, onResize) {
  const ro = new ResizeObserver(() => {
    const dpr = window.devicePixelRatio || 1;
    const { width, height } = canvas.getBoundingClientRect();
    canvas.width = Math.max(1, Math.round(width * dpr));
    canvas.height = Math.max(1, Math.round(height * dpr));
    onResize();
  });
  ro.observe(canvas);
  return ro;
}

/** Prepares a 2D context scaled to CSS pixels; returns {ctx, w, h}. */
export function begin(canvas) {
  const ctx = canvas.getContext('2d');
  const dpr = window.devicePixelRatio || 1;
  ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
  const w = canvas.width / dpr, h = canvas.height / dpr;
  ctx.clearRect(0, 0, w, h);
  return { ctx, w, h };
}

// Perceptual sequential map (inferno-like) and a diverging map, as RGB stops.
const SEQ = [
  [0.0, [0, 0, 4]], [0.13, [31, 12, 72]], [0.25, [85, 15, 109]], [0.38, [136, 34, 106]],
  [0.5, [186, 54, 85]], [0.63, [227, 89, 51]], [0.75, [249, 140, 10]], [0.88, [249, 201, 50]],
  [1.0, [252, 255, 164]],
];
const DIV = [
  [0.0, [33, 102, 172]], [0.25, [103, 169, 207]], [0.5, [247, 247, 247]],
  [0.75, [239, 138, 98]], [1.0, [178, 24, 43]],
];

function lut(stops) {
  const out = new Uint8ClampedArray(256 * 3);
  for (let i = 0; i < 256; i++) {
    const t = i / 255;
    let k = 0;
    while (k < stops.length - 2 && t > stops[k + 1][0]) k++;
    const [t0, c0] = stops[k], [t1, c1] = stops[k + 1];
    const f = (t - t0) / (t1 - t0);
    for (let j = 0; j < 3; j++) out[i * 3 + j] = c0[j] + (c1[j] - c0[j]) * f;
  }
  return out;
}

export const COLORMAPS = { sequential: lut(SEQ), diverging: lut(DIV) };

export function colorAt(map, t) {
  const i = Math.max(0, Math.min(255, Math.round(t * 255))) * 3;
  return [map[i], map[i + 1], map[i + 2]];
}

export const spl = (re, im) => 20 * Math.log10(Math.hypot(re, im) / 2e-5);
