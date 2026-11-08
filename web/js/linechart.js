// SPL along the receiver line: ray traced vs exact image-source solution.
import { autoSize, begin, cssVar, fmt, niceTicks } from './plot-utils.js';

export class LineChart {
  constructor(canvas, tip) {
    this.canvas = canvas;
    this.tip = tip;
    this.data = null;
    this.hoverIndex = -1;
    autoSize(canvas, () => this.draw());
    canvas.addEventListener('mousemove', (e) => this.hover(e));
    canvas.addEventListener('mouseleave', () => { this.hoverIndex = -1; this.tip.hidden = true; this.draw(); });
  }

  /** @param d {dist: Float64Array, rt: Float64Array|null, an: Float64Array|null, hits} (dB) */
  set(d) {
    this.data = d;
    this.draw();
  }

  frame(w, h) {
    const m = { l: 48, r: 14, t: 12, b: 36 };
    const { dist, rt, an } = this.data;
    let lo = Infinity, hi = -Infinity;
    for (const arr of [rt, an]) {
      if (!arr) continue;
      for (const v of arr) if (isFinite(v)) { lo = Math.min(lo, v); hi = Math.max(hi, v); }
    }
    if (!isFinite(lo)) { lo = 0; hi = 1; }
    // Keep deep nulls from flattening the curves: show at most 45 dB below the peak.
    lo = Math.max(lo, hi - 45);
    const pad = Math.max(1, (hi - lo) * 0.06);
    const yMin = Math.floor(lo - pad), yMax = Math.ceil(hi + pad);
    const xMin = dist[0], xMax = dist[dist.length - 1] || 1;
    const X = (x) => m.l + ((x - xMin) / (xMax - xMin || 1)) * (w - m.l - m.r);
    const Y = (y) => m.t + (1 - (y - yMin) / (yMax - yMin)) * (h - m.t - m.b);
    return { m, X, Y, xMin, xMax, yMin, yMax };
  }

  draw() {
    const { ctx, w, h } = begin(this.canvas);
    if (!this.data) return;
    const muted = cssVar('--muted'), grid = cssVar('--line');
    const { dist, rt, an } = this.data;
    const { m, X, Y, xMin, xMax, yMin, yMax } = this.frame(w, h);

    ctx.font = '11px system-ui, sans-serif';
    ctx.lineWidth = 1;
    ctx.fillStyle = muted;
    ctx.textAlign = 'right';
    ctx.textBaseline = 'middle';
    for (const t of niceTicks(yMin, yMax, Math.max(3, Math.floor((h - m.t - m.b) / 40)))) {
      const y = Math.round(Y(t)) + 0.5;
      ctx.strokeStyle = grid;
      ctx.beginPath(); ctx.moveTo(m.l, y); ctx.lineTo(w - m.r, y); ctx.stroke();
      ctx.fillText(String(t), m.l - 6, y);
    }
    ctx.textAlign = 'center';
    ctx.textBaseline = 'top';
    for (const t of niceTicks(xMin, xMax, Math.max(3, Math.floor((w - m.l - m.r) / 70)))) {
      ctx.fillRect(X(t), h - m.b, 1, 4);
      ctx.fillText(String(t), X(t), h - m.b + 6);
    }
    ctx.fillText('distance along line (m)', m.l + (w - m.l - m.r) / 2, h - 16);
    ctx.save();
    ctx.translate(12, m.t + (h - m.t - m.b) / 2);
    ctx.rotate(-Math.PI / 2);
    ctx.fillText('SPL (dB)', 0, -6);
    ctx.restore();

    ctx.save();
    ctx.beginPath();
    ctx.rect(m.l, m.t, w - m.l - m.r, h - m.t - m.b);
    ctx.clip();
    const series = (arr, color, width) => {
      ctx.strokeStyle = color;
      ctx.lineWidth = width;
      ctx.lineJoin = 'round';
      ctx.beginPath();
      let pen = false;
      for (let i = 0; i < arr.length; i++) {
        const v = isFinite(arr[i]) ? Math.max(arr[i], yMin - 5) : NaN;
        if (!isFinite(v)) { pen = false; continue; }
        if (pen) ctx.lineTo(X(dist[i]), Y(v)); else ctx.moveTo(X(dist[i]), Y(v));
        pen = true;
      }
      ctx.stroke();
    };
    if (an) series(an, cssVar('--an'), 2);
    if (rt) {
      series(rt, cssVar('--rt'), 1.5);
      ctx.fillStyle = cssVar('--rt');
      if (rt.length <= 160)
        for (let i = 0; i < rt.length; i++)
          if (isFinite(rt[i])) { ctx.beginPath(); ctx.arc(X(dist[i]), Y(rt[i]), 2.2, 0, 2 * Math.PI); ctx.fill(); }
    }
    if (this.hoverIndex >= 0) {
      const x = Math.round(X(dist[this.hoverIndex])) + 0.5;
      ctx.strokeStyle = muted;
      ctx.lineWidth = 1;
      ctx.setLineDash([3, 3]);
      ctx.beginPath(); ctx.moveTo(x, m.t); ctx.lineTo(x, h - m.b); ctx.stroke();
      ctx.setLineDash([]);
    }
    ctx.restore();
  }

  hover(e) {
    if (!this.data) return;
    const rect = this.canvas.getBoundingClientRect();
    const px = e.clientX - rect.left;
    const { X } = this.frame(rect.width, rect.height);
    const { dist, rt, an, hits } = this.data;
    let best = 0;
    for (let i = 1; i < dist.length; i++) if (Math.abs(X(dist[i]) - px) < Math.abs(X(dist[best]) - px)) best = i;
    this.hoverIndex = best;
    const r = rt?.[best], a = an?.[best];
    this.tip.textContent =
      `d ${dist[best].toFixed(3)} m\nray traced ${fmt(r, 2)} dB` +
      (hits ? ` (${hits[best]} hits)` : '') +
      `\nimage source ${fmt(a, 2)} dB\nΔ ${fmt(r - a, 2)} dB`;
    this.tip.hidden = false;
    const tw = this.tip.offsetWidth;
    this.tip.style.left = `${Math.min(px + 14, rect.width - tw - 4)}px`;
    this.tip.style.top = '14px';
    this.draw();
  }
}
