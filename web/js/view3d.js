// 3D view of the room: walls (opacity ~ |R|), source, receiver line, sample ray paths
// and the SPL slice as a textured plane. Physics coordinates are used directly (z up).
import * as THREE from 'three';
import { OrbitControls } from 'three/addons/controls/OrbitControls.js';
import { cssVar } from './plot-utils.js';

const BASES = [
  // slice normal axis -> in-plane basis (u, v) and normal, matching GridReceivers
  [[0, 1, 0], [0, 0, 1], [1, 0, 0]],
  [[1, 0, 0], [0, 0, 1], [0, -1, 0]],
  [[1, 0, 0], [0, 1, 0], [0, 0, 1]],
];

export class RoomView {
  constructor(host) {
    this.host = host;
    this.renderer = new THREE.WebGLRenderer({ antialias: true, alpha: true });
    this.renderer.setPixelRatio(window.devicePixelRatio || 1);
    host.appendChild(this.renderer.domElement);

    this.scene = new THREE.Scene();
    this.camera = new THREE.PerspectiveCamera(40, 1, 0.01, 500);
    this.camera.up.set(0, 0, 1);
    this.controls = new OrbitControls(this.camera, this.renderer.domElement);
    this.controls.enableDamping = true;
    this.controls.addEventListener('change', () => this.render());

    this.scene.add(new THREE.AmbientLight(0xffffff, 0.9));
    const sun = new THREE.DirectionalLight(0xffffff, 0.8);
    sun.position.set(3, -4, 8);
    this.scene.add(sun);

    this.groups = {};
    for (const k of ['room', 'source', 'line', 'slice', 'paths']) {
      this.groups[k] = new THREE.Group();
      this.scene.add(this.groups[k]);
    }
    this.texture = null;
    this.roomKey = '';

    new ResizeObserver(() => this.resize()).observe(host);
    this.resize();
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
    this.camera.aspect = width / height;
    this.camera.updateProjectionMatrix();
    this.render();
  }

  render() { this.renderer.render(this.scene, this.camera); }

  static clear(group) {
    for (const c of [...group.children]) {
      group.remove(c);
      c.geometry?.dispose();
      if (c.material && c.material !== RoomView.sharedMaterial) c.material.dispose();
    }
  }

  /** Rebuilds geometry for a scene description (see scene.js). */
  setScene(s) {
    const { L, W, H } = s.room;
    const accent = new THREE.Color(cssVar('--accent'));
    const rcv = new THREE.Color(cssVar('--rcv'));
    const edge = new THREE.Color(cssVar('--muted'));
    const size = Math.max(L, W, H);

    // room edges + walls
    const g = this.groups.room;
    RoomView.clear(g);
    const box = new THREE.LineSegments(
      new THREE.EdgesGeometry(new THREE.BoxGeometry(L, W, H)),
      new THREE.LineBasicMaterial({ color: edge }),
    );
    box.position.set(L / 2, W / 2, H / 2);
    g.add(box);
    const wallDefs = [
      // [width, height, position, basis X, basis Y]
      [L, W, [L / 2, W / 2, 0], [1, 0, 0], [0, 1, 0]],          // bottom
      [W, H, [0, W / 2, H / 2], [0, 1, 0], [0, 0, 1]],          // left
      [L, H, [L / 2, 0, H / 2], [1, 0, 0], [0, 0, 1]],          // front
      [W, H, [L, W / 2, H / 2], [0, 1, 0], [0, 0, 1]],          // right
      [L, H, [L / 2, W, H / 2], [1, 0, 0], [0, 0, 1]],          // back
      [L, W, [L / 2, W / 2, H], [1, 0, 0], [0, 1, 0]],          // top
    ];
    wallDefs.forEach(([w, h, pos, bx, by], i) => {
      const mag = s.walls[i].mag;
      if (mag <= 0) return;
      const mesh = new THREE.Mesh(
        new THREE.PlaneGeometry(w, h),
        new THREE.MeshBasicMaterial({
          color: accent, transparent: true, opacity: 0.05 + 0.18 * mag,
          side: THREE.DoubleSide, depthWrite: false,
        }),
      );
      orient(mesh, bx, by);
      mesh.position.set(...pos);
      g.add(mesh);
    });

    // source
    const src = this.groups.source;
    RoomView.clear(src);
    const ball = new THREE.Mesh(
      new THREE.SphereGeometry(size * 0.014, 24, 16),
      new THREE.MeshStandardMaterial({ color: accent, emissive: accent, emissiveIntensity: 0.4 }),
    );
    ball.position.set(s.source.x, s.source.y, s.source.z);
    src.add(ball);

    // receiver line
    const ln = this.groups.line;
    RoomView.clear(ln);
    const l = s.line;
    const pts = [];
    for (let i = 0; i < l.n; i++) {
      const t = l.n > 1 ? i / (l.n - 1) : 0;
      pts.push(l.x0 + (l.x1 - l.x0) * t, l.y0 + (l.y1 - l.y0) * t, l.z0 + (l.z1 - l.z0) * t);
    }
    const lg = new THREE.BufferGeometry();
    lg.setAttribute('position', new THREE.Float32BufferAttribute(pts, 3));
    ln.add(new THREE.Line(lg, new THREE.LineBasicMaterial({ color: rcv })));
    ln.add(new THREE.Points(lg.clone(), new THREE.PointsMaterial({ color: rcv, size: Math.max(3, 6 - l.n / 60), sizeAttenuation: false })));

    // slice plane (texture filled in by setSliceImage)
    const sl = this.groups.slice;
    RoomView.clear(sl);
    const dims = [L, W, H];
    const [bu, bv] = BASES[s.slice.axis];
    const uLen = dims[bu.findIndex((c) => c !== 0)], vLen = dims[bv.findIndex((c) => c !== 0)];
    this.sliceMaterial = new THREE.MeshBasicMaterial({
      transparent: true, opacity: 0.92, side: THREE.DoubleSide, depthWrite: false,
      color: 0xffffff, map: this.texture,
    });
    const plane = new THREE.Mesh(new THREE.PlaneGeometry(uLen, vLen), this.sliceMaterial);
    orient(plane, bu, bv);
    const center = [L / 2, W / 2, H / 2];
    center[s.slice.axis] = s.slice.offset;
    plane.position.set(...center);
    plane.visible = !!this.texture;
    this.slicePlane = plane;
    sl.add(plane);

    // camera: refit only when the room size changes
    const key = `${L}x${W}x${H}`;
    if (key !== this.roomKey) {
      this.roomKey = key;
      this.controls.target.set(L / 2, W / 2, H / 2);
      const d = Math.hypot(L, W, H);
      this.camera.position.set(L / 2 + d * 0.95, W / 2 - d * 1.15, H / 2 + d * 0.75);
      this.camera.near = d / 200;
      this.camera.far = d * 20;
      this.camera.updateProjectionMatrix();
      this.controls.update();
    }
    this.render();
  }

  /** Uses the heatmap's offscreen canvas as the slice texture. */
  setSliceImage(canvas) {
    if (!this.texture || this.texture.image !== canvas) {
      this.texture?.dispose();
      this.texture = new THREE.CanvasTexture(canvas);
      this.texture.magFilter = THREE.NearestFilter;
      this.texture.minFilter = THREE.LinearFilter;
      this.texture.colorSpace = THREE.SRGBColorSpace;
    }
    this.texture.needsUpdate = true;
    if (this.sliceMaterial) {
      this.sliceMaterial.map = this.texture;
      this.sliceMaterial.needsUpdate = true;
      this.slicePlane.visible = true;
    }
    this.render();
  }

  /** Ray polylines as x,y,z triples separated by NaN triples. Later segments fade. */
  setPaths(flat, maxOrder) {
    const g = this.groups.paths;
    RoomView.clear(g);
    if (!flat || flat.length === 0) { this.render(); return; }
    const accent = new THREE.Color(cssVar('--accent'));
    const fade = new THREE.Color(cssVar('--muted'));
    const pos = [], col = [];
    let k = 0; // segment index within the current ray
    for (let i = 3; i < flat.length; i += 3) {
      const a = i - 3;
      if (Number.isNaN(flat[i]) || Number.isNaN(flat[a])) { k = 0; continue; }
      pos.push(flat[a], flat[a + 1], flat[a + 2], flat[i], flat[i + 1], flat[i + 2]);
      const c = accent.clone().lerp(fade, Math.min(1, k / Math.max(1, maxOrder + 1)));
      col.push(c.r, c.g, c.b, c.r, c.g, c.b);
      k++;
    }
    const geo = new THREE.BufferGeometry();
    geo.setAttribute('position', new THREE.Float32BufferAttribute(pos, 3));
    geo.setAttribute('color', new THREE.Float32BufferAttribute(col, 3));
    g.add(new THREE.LineSegments(geo, new THREE.LineBasicMaterial({ vertexColors: true, transparent: true, opacity: 0.45 })));
    this.render();
  }
}

function orient(mesh, bx, by) {
  const X = new THREE.Vector3(...bx), Y = new THREE.Vector3(...by);
  const Z = new THREE.Vector3().crossVectors(X, Y);
  mesh.quaternion.setFromRotationMatrix(new THREE.Matrix4().makeBasis(X, Y, Z));
}
