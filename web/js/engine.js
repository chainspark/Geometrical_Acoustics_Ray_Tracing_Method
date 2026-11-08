// Pool of Web Workers, each with its own WebAssembly instance. A run hands out
// ray ranges dynamically (idle workers take the next chunk) and adds the partial
// complex pressure sums together as they arrive.

class WorkerHandle {
  constructor() {
    this.worker = new Worker(new URL('./worker.js', import.meta.url), { type: 'module' });
    this.pending = new Map();
    this.nextId = 1;
    this.worker.onmessage = ({ data }) => {
      const p = this.pending.get(data.id);
      if (!p) return;
      this.pending.delete(data.id);
      if (data.type === 'error') p.reject(new Error(data.error));
      else p.resolve(data);
    };
    this.worker.onerror = (e) => {
      for (const p of this.pending.values()) p.reject(new Error(e.message || 'worker failed'));
      this.pending.clear();
    };
  }
  call(msg) {
    const id = this.nextId++;
    return new Promise((resolve, reject) => {
      this.pending.set(id, { resolve, reject });
      this.worker.postMessage({ ...msg, id });
    });
  }
  terminate() { this.worker.terminate(); }
}

export class Engine {
  constructor() {
    this.workers = [];
    this.runId = 0;
  }

  async setWorkerCount(n) {
    while (this.workers.length > n) this.workers.pop().terminate();
    while (this.workers.length < n) this.workers.push(new WorkerHandle());
    await Promise.all(this.workers.map((w) => w.call({ type: 'init' })));
  }

  /** Stops the current run; chunks already in flight are discarded. */
  cancel() { this.runId++; }

  /**
   * Traces `scene` and reports progress.
   * @param scene    scene description from scene.js
   * @param onReference({grid, line, paths, images})  exact solution + ray paths
   * @param onProgress(sums, raysDone)                 called after every chunk
   * @returns {Promise<'done'|'cancelled'>}
   */
  async run(scene, { onReference, onProgress, paths = 0 }) {
    const runId = ++this.runId;
    const live = () => runId === this.runId;

    const results = await Promise.all(this.workers.map((w) => w.call({ type: 'configure', scene })));
    const error = results.find((r) => r.error)?.error;
    if (error) throw new Error(error);
    if (!live()) return 'cancelled';

    const G = scene.slice.nu * scene.slice.nv;
    const L = scene.line.n;
    const sums = {
      grid: new Float64Array(2 * G), gridHits: new Float64Array(G),
      line: new Float64Array(2 * L), lineHits: new Float64Array(L),
    };
    const total = scene.rays;
    let next = 0;
    let done = 0;

    const loop = async (w, first) => {
      if (first) {
        const ref = await w.call({ type: 'reference', paths });
        if (live()) onReference(ref);
      }
      let chunk = 20000;
      while (live() && next < total) {
        const begin = next;
        const end = Math.min(total, begin + chunk);
        next = end;
        const t0 = performance.now();
        const r = await w.call({ type: 'trace', begin, end });
        if (!live()) return;
        add(sums.grid, r.grid); add(sums.gridHits, r.gridHits);
        add(sums.line, r.line); add(sums.lineHits, r.lineHits);
        done += end - begin;
        onProgress(sums, done);
        // Aim for ~120 ms per chunk so cancellation and redraws stay responsive.
        const rate = (end - begin) / Math.max(1, performance.now() - t0);
        chunk = Math.round(Math.min(2e6, Math.max(5000, rate * 120)));
      }
    };

    await Promise.all(this.workers.map((w, i) => loop(w, i === 0)));
    return live() ? 'done' : 'cancelled';
  }
}

function add(into, from) {
  for (let i = 0; i < into.length; i++) into[i] += from[i];
}
