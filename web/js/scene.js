// UI state, presets, and conversion to the scene description the workers consume.

export const WALL_NAMES = ['bottom', 'left', 'front', 'right', 'back', 'top'];

const wall = (mag = 0, phaseDeg = 0) => ({ mag, phaseDeg });
const walls = (bottom = 0, rest = 0, top = rest) =>
  [wall(bottom), wall(rest), wall(rest), wall(rest), wall(rest), wall(top)];

const base = {
  raysExp: 6.5, maxOrder: 1, radius: 0.05, seed: 1, paths: 120, autoRun: true,
};

export const PRESETS = {
  case2: {
    label: 'Case II · floor reflection, horizontal sweep',
    state: {
      ...base,
      room: { L: 10, W: 10, H: 4 }, walls: walls(1),
      source: { x: 5, y: 5, z: 1, freq: 250, powerDb: 100 },
      slice: { axis: 1, offset: 5, res: 12 },
      line: { x0: 5.05, y0: 5, z0: 2, x1: 9.95, y1: 5, z1: 2, n: 99 },
    },
  },
  case1: {
    label: 'Case I · floor reflection, vertical sweep',
    state: {
      ...base,
      room: { L: 4, W: 4, H: 4 }, walls: walls(1),
      source: { x: 2, y: 2, z: 3, freq: 250, powerDb: 100 },
      slice: { axis: 1, offset: 2, res: 20 },
      line: { x0: 2, y0: 2, z0: 0.05, x1: 2, y1: 2, z1: 2.95, n: 59 },
    },
  },
  floorMap: {
    label: 'Floor reflection · horizontal SPL map',
    state: {
      ...base,
      room: { L: 10, W: 10, H: 4 }, walls: walls(1),
      source: { x: 5, y: 5, z: 1, freq: 500, powerDb: 100 },
      slice: { axis: 2, offset: 2, res: 10 },
      line: { x0: 5.05, y0: 5, z0: 2, x1: 9.95, y1: 5, z1: 2, n: 99 },
    },
  },
  anechoic: {
    label: 'Anechoic · free field',
    state: {
      ...base, maxOrder: 0,
      room: { L: 4, W: 4, H: 4 }, walls: walls(0),
      source: { x: 2, y: 2, z: 3, freq: 250, powerDb: 100 },
      slice: { axis: 1, offset: 2, res: 20 },
      line: { x0: 2, y0: 2, z0: 2.9, x1: 2, y1: 2, z1: 0.1, n: 57 },
    },
  },
  reverberant: {
    label: 'Reverberant room · 6th order',
    state: {
      ...base, maxOrder: 6, raysExp: 7,
      room: { L: 6, W: 4, H: 3 }, walls: walls(0.8, 0.7, 0.6),
      source: { x: 1.5, y: 1.2, z: 1.4, freq: 125, powerDb: 100 },
      slice: { axis: 2, offset: 1.2, res: 10 },
      line: { x0: 0.3, y0: 0.3, z0: 1.2, x1: 5.7, y1: 3.7, z1: 1.2, n: 120 },
    },
  },
  parallel: {
    label: 'Floor + ceiling · 1 kHz, 4th order',
    state: {
      ...base, maxOrder: 4, raysExp: 7,
      room: { L: 10, W: 10, H: 4 },
      walls: [wall(1), wall(0), wall(0), wall(0), wall(0), wall(0.9, 180)],
      source: { x: 5, y: 5, z: 1.5, freq: 1000, powerDb: 100 },
      slice: { axis: 1, offset: 5, res: 16 },
      line: { x0: 5.05, y0: 5, z0: 2.2, x1: 9.95, y1: 5, z1: 2.2, n: 160 },
    },
  },
};

export const DEFAULT_PRESET = 'case2';

export const clone = (o) => structuredClone(o);

/** Number of rays for a slider exponent, rounded to 2 significant digits. */
export function raysFromExp(exp) {
  const n = 10 ** exp;
  const mag = 10 ** Math.floor(Math.log10(n) - 1);
  return Math.round(n / mag) * mag;
}

export function formatCount(n) {
  if (n >= 1e6) return `${+(n / 1e6).toFixed(n >= 1e7 ? 0 : 1)}M`;
  if (n >= 1e3) return `${+(n / 1e3).toFixed(n >= 1e4 ? 0 : 1)}k`;
  return String(n);
}

/** In-plane axes of a slice normal to `axis` (u < v), matching the C++ GridReceivers. */
export function sliceAxes(axis) {
  return { u: axis === 0 ? 1 : 0, v: axis === 2 ? 1 : 2 };
}

const roomSize = (room) => [room.L, room.W, room.H];

/** Scene description sent to the workers. */
export function toScene(state) {
  const size = roomSize(state.room);
  const { u, v } = sliceAxes(+state.slice.axis);
  const cells = (len) => Math.max(2, Math.min(400, Math.round(len * state.slice.res)));
  return {
    room: { ...state.room },
    walls: state.walls.map((w) => ({ mag: +w.mag, phaseDeg: +w.phaseDeg })),
    source: { ...state.source },
    rays: raysFromExp(state.raysExp),
    maxOrder: Math.round(state.maxOrder),
    seed: Math.round(state.seed),
    radius: state.radius,
    slice: { axis: +state.slice.axis, offset: state.slice.offset, nu: cells(size[u]), nv: cells(size[v]) },
    line: { ...state.line, n: Math.max(2, Math.round(state.line.n)) },
  };
}

/** Human-readable problems with the state, or [] if it can be run. */
export function check(state) {
  const issues = [];
  const size = roomSize(state.room);
  const s = state.source;
  if (size.some((d) => !(d > 0))) issues.push('Room dimensions must be positive.');
  [s.x, s.y, s.z].forEach((c, i) => {
    if (!(c > 0 && c < size[i])) issues.push(`Source ${'xyz'[i]} must be inside the room (0, ${size[i]}).`);
  });
  const ax = +state.slice.axis;
  if (!(state.slice.offset >= 0 && state.slice.offset <= size[ax])) issues.push('Map slice is outside the room.');
  if (!(state.radius > 0)) issues.push('Receiver radius must be positive.');
  if (!(s.freq > 0)) issues.push('Frequency must be positive.');
  return issues;
}

export function getPath(obj, path) {
  return path.split('.').reduce((o, k) => (o == null ? undefined : o[k]), obj);
}

export function setPath(obj, path, value) {
  const keys = path.split('.');
  const last = keys.pop();
  const target = keys.reduce((o, k) => o[k], obj);
  target[last] = value;
}
