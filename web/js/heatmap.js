// SPL map on the slice plane (2D canvas), plus an offscreen image reused as the 3D texture.
import { autoSize, begin, colorAt, COLORMAPS, cssVar, fmt, niceTicks } from './plot-utils.js';

const AXIS = ['x', 'y', 'z'];

export class Heatmap {
  constructor(canvas, tip) {
    this.canvas = canvas;
    this.tip = tip;
    this.data = null;
    this.image = document.createElement('canvas'); // nu x nv pixels
    this.onImage = () => {};
    autoSize(canvas, () => this.draw());
    canvas.addEventListener('mousemove', (e) => this.hover(e));
    canvas.addEventListener('mouseleave', () => { this.tip.hidden = true; });
  }

  /**
   * @param d {nu, nv, uAxis, vAxis, uSize, vSize, values: Float64Array (dB), hits?, mode, marker?: [u, v]}
   *          mode: 'spl' (sequential colours) or 'diff' (diverging, +/- range)
   */
  set(d) {
    this.data = d;
    this.range = this.computeRange(d);
    this.paintImage();
    this.draw();
  }

  computeRange(d) {
    if (d.mode === 'diff') return [-3, 3];
    let max = -Infinity, min = Infinity;
    for (const v of d.values) if (isFinite(v)) { max = Math.max(max, v); min = Math.min(min, v); }
    if (!isFinite(max)) return [0, 1];
    max = Math.ceil(max);
    return [Math.max(min, max - 40), max];
  }

  norm(v) {
    const [lo, hi] = this.range;
    return (v - lo) / (hi - lo || 1);
  }

  paintImage() {
    const { nu, nv, values, mode } = this.data;
    const map = mode === 'diff' ? COLORMAPS.diverging : COLORMAPS.sequential;
    this.image.width = nu;
    this.image.height = nv;
    const ctx = this.image.getContext('2d');
    const img = ctx.createImageData(nu, nv);
    for (let iv = 0; iv < nv; iv++) {
      for (let iu = 0; iu < nu; iu++) {
        const v = values[iv * nu + iu];
        const o = ((nv - 1 - iv) * nu + iu) * 4; // v increases upwards
        if (!isFinite(v)) { img.data[o + 3] = 0; continue; }
        const [r, g, b] = colorAt(map, this.norm(v));
        img.data[o] = r; img.data[o + 1] = g; img.data[o + 2] = b; img.data[o + 3] = 255;
      }
    }
    ctx.putImageData(img, 0, 0);
    this.onImage(this.image);
  }

  layout(w, h) {
    const m = { l: 46, r: 64, t: 12, b: 36 };
    const { uSize, vSize } = this.data;
    const aw = w - m.l - m.r, ah = h - m.t - m.b;
    const s = Math.max(1, Math.min(aw / uSize, ah / vSize));
    const pw = uSize * s, ph = vSize * s;
    const x0 = m.l + (aw - pw) / 2, y0 = m.t + (ah - ph) / 2;
    return { x0, y0, pw, ph, s };
  }

  draw() {
    const { ctx, w, h } = begin(this.canvas);
    if (!this.data) return;
    const ink = cssVar('--ink'), muted = cssVar('--muted'), line = cssVar('--line');
    const { x0, y0, pw, ph } = this.layout(w, h);
    const { uSize, vSize, uAxis, vAxis, mode } = this.data;

    ctx.fillStyle = line;
    ctx.fillRect(x0, y0, pw, ph);
    ctx.imageSmoothingEnabled = false;
    ctx.drawImage(this.image, x0, y0, pw, ph);
    ctx.strokeStyle = muted;
    ctx.lineWidth = 1;
    ctx.strokeRect(x0 + 0.5, y0 + 0.5, pw - 1, ph - 1);

    // axes
    ctx.fillStyle = muted;
    ctx.font = '11px system-ui, sans-serif';
    ctx.textAlign = 'center';
    ctx.textBaseline = 'top';
    for (const t of niceTicks(0, uSize, Math.max(2, Math.floor(pw / 60)))) {
      const x = x0 + (t / uSize) * pw;
      ctx.fillRect(x, y0 + ph, 1, 4);
      ctx.fillText(String(t), x, y0 + ph + 6);
    }
    ctx.fillText(`${AXIS[uAxis]} (m)`, x0 + pw / 2, y0 + ph + 20);
    ctx.textAlign = 'right';
    ctx.textBaseline = 'middle';
    for (const t of niceTicks(0, vSize, Math.max(2, Math.floor(ph / 40)))) {
      const y = y0 + ph - (t / vSize) * ph;
      ctx.fillRect(x0 - 4, y, 4, 1);
      ctx.fillText(String(t), x0 - 7, y);
    }
    ctx.save();
    ctx.translate(12, y0 + ph / 2);
    ctx.rotate(-Math.PI / 2);
    ctx.textAlign = 'center';
    ctx.fillText(`${AXIS[vAxis]} (m)`, 0, 0);
    ctx.restore();

    // source marker (projected onto the slice)
    if (this.data.marker) {
      const [mu, mv] = this.data.marker;
      ctx.beginPath();
      ctx.arc(x0 + (mu / uSize) * pw, y0 + ph - (mv / vSize) * ph, 5, 0, 2 * Math.PI);
      ctx.strokeStyle = '#ffffff';
      ctx.lineWidth = 2.5;
      ctx.stroke();
      ctx.strokeStyle = cssVar('--accent');
      ctx.lineWidth = 1.5;
      ctx.stroke();
    }

    // colour bar
    const map = mode === 'diff' ? COLORMAPS.diverging : COLORMAPS.sequential;
    const cbx = x0 + pw + 14, cbw = 10, cby = y0, cbh = ph;
    for (let i = 0; i < cbh; i++) {
      const [r, g, b] = colorAt(map, 1 - i / cbh);
      ctx.fillStyle = `rgb(${r},${g},${b})`;
      ctx.fillRect(cbx, cby + i, cbw, 1.5);
    }
    const [lo, hi] = this.range;
    ctx.fillStyle = muted;
    ctx.textAlign = 'left';
    for (const t of niceTicks(lo, hi, 5)) {
      const y = cby + cbh - ((t - lo) / (hi - lo)) * cbh;
      ctx.fillText(mode === 'diff' ? (t > 0 ? `+${t}` : String(t)) : String(t), cbx + cbw + 4, y);
    }
    ctx.fillStyle = ink;
    ctx.textBaseline = 'top';
    ctx.fillText('dB', cbx, cby + cbh + 6);
  }

  hover(e) {
    if (!this.data) return;
    const rect = this.canvas.getBoundingClientRect();
    const px = e.clientX - rect.left, py = e.clientY - rect.top;
    const { x0, y0, pw, ph } = this.layout(rect.width, rect.height);
    const fu = (px - x0) / pw, fv = 1 - (py - y0) / ph;
    if (fu < 0 || fu >= 1 || fv < 0 || fv >= 1) { this.tip.hidden = true; return; }
    const { nu, nv, uSize, vSize, uAxis, vAxis, values, hits, mode } = this.data;
    const iu = Math.floor(fu * nu), iv = Math.floor(fv * nv);
    const i = iv * nu + iu;
    const cu = ((iu + 0.5) / nu) * uSize, cv = ((iv + 0.5) / nv) * vSize;
    const label = mode === 'diff' ? 'Δ' : 'SPL';
    this.tip.textContent =
      `${AXIS[uAxis]} ${cu.toFixed(2)}  ${AXIS[vAxis]} ${cv.toFixed(2)} m\n${label} ${fmt(values[i], 2)} dB` +
      (hits ? `\nhits ${hits[i]}` : '');
    this.tip.hidden = false;
    const tw = this.tip.offsetWidth;
    this.tip.style.left = `${Math.min(px + 14, rect.width - tw - 4)}px`;
    this.tip.style.top = `${Math.max(4, py - 46)}px`;
  }
}
