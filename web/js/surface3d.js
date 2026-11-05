// 3D surface plot of the SPL map: height and colour = SPL, rotatable with the mouse.
import * as THREE from 'three';
import { OrbitControls } from 'three/addons/controls/OrbitControls.js';
import { CSS2DObject, CSS2DRenderer } from 'three/addons/renderers/CSS2DRenderer.js';
import { COLORMAPS, cssVar, niceTicks } from './plot-utils.js';

const AXIS = ['x', 'y', 'z'];

// Colour maps converted once to linear RGB (vertex colours are linear in three.js).
const LINEAR = {};
for (const [name, lut] of Object.entries(COLORMAPS)) {
  const out = new Float32Array(lut.length);
  const c = new THREE.Color();
  for (let i = 0; i < lut.length; i += 3) {
    c.setRGB(lut[i] / 255, lut[i + 1] / 255, lut[i + 2] / 255, THREE.SRGBColorSpace);
    out[i] = c.r; out[i + 1] = c.g; out[i + 2] = c.b;
  }
  LINEAR[name] = out;
}

export class SplSurface {
  constructor(host) {
    this.host = host;
    this.renderer = new THREE.WebGLRenderer({ antialias: true, alpha: true });
    this.renderer.setPixelRatio(window.devicePixelRatio || 1);
    this.renderer.domElement.style.position = 'absolute';
    this.renderer.domElement.style.inset = '0';
    host.appendChild(this.renderer.domElement);

    this.labels = new CSS2DRenderer();
    Object.assign(this.labels.domElement.style, { position: 'absolute', inset: '0', pointerEvents: 'none' });
    host.appendChild(this.labels.domElement);

    this.scene = new THREE.Scene();
    this.camera = new THREE.PerspectiveCamera(35, 1, 0.01, 1000);
    this.camera.up.set(0, 0, 1);
    this.controls = new OrbitControls(this.camera, this.renderer.domElement);
    this.controls.enableDamping = true;
    this.controls.addEventListener('change', () => this.render());

    this.scene.add(new THREE.AmbientLight(0xffffff, 2.2));
    const key = new THREE.DirectionalLight(0xffffff, 0.9);
    key.position.set(-0.6, -1, 1.4);
    this.scene.add(key);

    this.material = new THREE.MeshLambertMaterial({ vertexColors: true, side: THREE.DoubleSide });
    this.mesh = null;
    this.frame = new THREE.Group();
    this.scene.add(this.frame);
    this.layoutKey = '';

    new ResizeObserver(() => this.resize()).observe(host);
    const tick = () => {
      if (this.controls.update()) this.render();
      requestAnimationFrame(tick);
    };
    requestAnimationFrame(tick);
  }

  resize() {
    const { width, height } = this.host.getBoundingClientRect();
    if (!width || !height) return;
    this.renderer.setSize(width, height, false);
    this.renderer.domElement.style.width = '100%';
    this.renderer.domElement.style.height = '100%';
    this.labels.setSize(width, height);
    this.camera.aspect = width / height;
    this.camera.updateProjectionMatrix();
    this.render();
  }

  render() {
    this.renderer.render(this.scene, this.camera);
    this.labels.render(this.scene, this.camera);
  }

  /**
   * @param d {nu, nv, uAxis, vAxis, uSize, vSize, values: Float64Array (dB), mode: 'spl'|'diff'}
   * @param range [lo, hi] dB, shared with the 2D map so both use the same colours
   */
  set(d, range) {
    const { nu, nv, uSize, vSize, values, mode } = d;
    const [lo, hi] = range;
    const height = 0.45 * Math.max(uSize, vSize); // world height of the full dB range
    const key = `${nu}x${nv}:${uSize}x${vSize}:${d.uAxis}${d.vAxis}:${lo}:${hi}:${mode}`;

    if (!this.mesh || this.mesh.userData.nu !== nu || this.mesh.userData.nv !== nv) {
      this.mesh?.geometry.dispose();
      if (this.mesh) this.scene.remove(this.mesh);
      this.mesh = new THREE.Mesh(gridGeometry(nu, nv, uSize, vSize), this.material);
      this.mesh.userData = { nu, nv };
      this.scene.add(this.mesh);
    }

    // Heights and colours. Cells with no result sit on the floor of the range.
    const geo = this.mesh.geometry;
    const pos = geo.attributes.position.array;
    const col = geo.attributes.color.array;
    const lut = mode === 'diff' ? LINEAR.diverging : LINEAR.sequential;
    for (let iv = 0; iv < nv; iv++) {
      for (let iu = 0; iu < nu; iu++) {
        const i = iv * nu + iu;
        const v = values[i];
        const t = Number.isFinite(v) ? Math.min(1, Math.max(0, (v - lo) / (hi - lo || 1))) : 0;
        pos[i * 3] = ((iu + 0.5) / nu) * uSize;
        pos[i * 3 + 1] = ((iv + 0.5) / nv) * vSize;
        pos[i * 3 + 2] = t * height;
        const k = Math.round(t * 255) * 3;
        col[i * 3] = lut[k]; col[i * 3 + 1] = lut[k + 1]; col[i * 3 + 2] = lut[k + 2];
      }
    }
    geo.attributes.position.needsUpdate = true;
    geo.attributes.color.needsUpdate = true;
    geo.computeVertexNormals();
    geo.computeBoundingSphere();

    if (key !== this.layoutKey) {
      const refit = !this.layoutKey.startsWith(`${nu}x${nv}:${uSize}x${vSize}:`);
      this.layoutKey = key;
      this.buildFrame(d, lo, hi, height, mode);
      if (refit) this.fitCamera(uSize, vSize, height);
    }
    this.render();
  }

  buildFrame(d, lo, hi, height, mode) {
    for (const c of [...this.frame.children]) {
      this.frame.remove(c);
      c.geometry?.dispose();
      c.material?.dispose();
      c.element?.remove();
    }
    const { uSize, vSize, uAxis, vAxis } = d;
    const lineMat = new THREE.LineBasicMaterial({ color: new THREE.Color(cssVar('--muted')), transparent: true, opacity: 0.55 });
    const pts = [];
    const seg = (a, b) => pts.push(...a, ...b);

    // Floor grid, plus dB gridlines on the two walls away from the default camera
    // (y = vSize and x = uSize), so the grid never covers the surface.
    const uTicks = niceTicks(0, uSize, 5), vTicks = niceTicks(0, vSize, 5), zTicks = niceTicks(lo, hi, 4);
    const Z = (db) => ((db - lo) / (hi - lo || 1)) * height;
    for (const u of uTicks) { seg([u, 0, 0], [u, vSize, 0]); seg([u, vSize, 0], [u, vSize, height]); }
    for (const v of vTicks) { seg([0, v, 0], [uSize, v, 0]); seg([uSize, v, 0], [uSize, v, height]); }
    for (const db of zTicks) { seg([0, vSize, Z(db)], [uSize, vSize, Z(db)]); seg([uSize, 0, Z(db)], [uSize, vSize, Z(db)]); }
    seg([0, 0, 0], [uSize, 0, 0]); seg([0, 0, 0], [0, vSize, 0]);
    seg([uSize, 0, 0], [uSize, 0, height]); seg([uSize, vSize, 0], [uSize, vSize, height]);
    seg([0, vSize, 0], [0, vSize, height]);
    const g = new THREE.BufferGeometry();
    g.setAttribute('position', new THREE.Float32BufferAttribute(pts, 3));
    this.frame.add(new THREE.LineSegments(g, lineMat));

    const off = 0.06 * Math.max(uSize, vSize);
    const label = (text, p, cls = '') => {
      const el = document.createElement('div');
      el.className = `axis-label ${cls}`;
      el.textContent = text;
      const o = new CSS2DObject(el);
      o.position.set(...p);
      this.frame.add(o);
    };
    // u along the front edge, v along the left edge, dB up the front-right corner.
    for (const u of uTicks) label(String(u), [u, -off, 0]);
    for (const v of vTicks) label(String(v), [-off, v, 0]);
    for (const db of zTicks) label(mode === 'diff' && db > 0 ? `+${db}` : String(db), [uSize + off * 0.7, -off * 0.7, Z(db)]);
    label(`${AXIS[uAxis]} (m)`, [uSize / 2, -off * 2.3, 0], 'title');
    label(`${AXIS[vAxis]} (m)`, [-off * 2.3, vSize / 2, 0], 'title');
    label(mode === 'diff' ? 'Δ dB' : 'SPL (dB)', [uSize + off * 0.7, -off * 0.7, height + off], 'title');
  }

  fitCamera(uSize, vSize, height) {
    const c = new THREE.Vector3(uSize / 2, vSize / 2, height * 0.4);
    const d = Math.hypot(uSize, vSize, height);
    this.controls.target.copy(c);
    this.camera.position.set(c.x - d * 0.64, c.y - d * 1.1, c.z + d * 0.72);
    this.camera.near = d / 200;
    this.camera.far = d * 20;
    this.camera.updateProjectionMatrix();
    this.controls.update();
  }
}

/** nu x nv vertex grid (one vertex per cell centre) with an index buffer and colour attribute. */
function gridGeometry(nu, nv) {
  const geo = new THREE.BufferGeometry();
  geo.setAttribute('position', new THREE.BufferAttribute(new Float32Array(nu * nv * 3), 3));
  geo.setAttribute('color', new THREE.BufferAttribute(new Float32Array(nu * nv * 3), 3));
  const idx = [];
  for (let iv = 0; iv < nv - 1; iv++) {
    for (let iu = 0; iu < nu - 1; iu++) {
      const a = iv * nu + iu, b = a + 1, c = a + nu, d = c + 1;
      idx.push(a, b, d, a, d, c);
    }
  }
  geo.setIndex(idx);
  return geo;
}
