const { build } = require('./netlist');
const RAIL_PULL = parseFloat(process.env.RAIL_PULL || '0.35');
// band layout (default since rev 2): +5 V along the top and bottom rows joined down the right edge, GND along the middle row
const BANDS = process.env.BANDS !== '0';
const MAXHOP = parseInt(process.env.MAXHOP || '2', 10);
const IMPROVE_PASSES = parseInt(process.env.IMPROVE || '4', 10);
const HOPCOST = parseFloat(process.env.HOPCOST || '10');
// analog rules: noisy (charge pump) parts are pushed at least SEP_D holes away from sensitive parts
const SEP_D = parseFloat(process.env.SEP_D || '7');
const SEP_W = parseFloat(process.env.SEP_W || '1.5');
const NEAR_W = parseFloat(process.env.NEAR_W || '1');

// ---------- footprints ----------
const SPAN = { R: 4, D: 3, C: 2, E: 2 };
const DIRS = [[1, 0], [0, 1], [-1, 0], [0, -1]];
const PIN_NAMES = { R: ['1', '2'], D: ['A', 'K'], C: ['1', '2'], E: ['+', '-'] };

function footprint(p, x, y, o) {
  const pins = {};
  const cells = [];
  if (SPAN[p.kind]) {
    const s = SPAN[p.kind];
    const d = DIRS[o];
    const names = PIN_NAMES[p.kind];
    pins[names[0]] = [x, y];
    pins[names[1]] = [x + d[0] * s, y + d[1] * s];
    for (let i = 0; i <= s; i++) {
      const cx = x + d[0] * i, cy = y + d[1] * i;
      cells.push([cx, cy]);
      if (p.kind === 'E') {
        cells.push([cx + d[1], cy + d[0]], [cx - d[1], cy - d[0]]);
      }
    }
  } else if (p.kind === "INA" || p.kind === "DIP8") {
    // adapter boards: two pin rows 3 holes apart; o rotates the part in 90 degree steps about pin 1
    const rot = (dx, dy) => (o === 0 ? [dx, dy] : o === 1 ? [-dy, dx] : o === 2 ? [-dx, -dy] : [dy, -dx]);
    const rows = p.kind === "INA" ? 7 : 4;
    const left = rows, total = rows * 2;
    for (let n = 1; n <= total; n++) {
      const base = n <= left ? [0, n - 1] : [3, total - n];
      const [rx, ry] = rot(base[0], base[1]);
      pins[n] = [x + rx, y + ry];
    }
    for (let i = 0; i < 4; i++) for (let j = 0; j < rows; j++) {
      const [rx, ry] = rot(i, j);
      cells.push([x + rx, y + ry]);
    }
  } else {
    pins.P = [x, y];
    cells.push([x, y]);
  }
  return { pins, cells };
}

// ---------- RNG ----------
function rng(seed) {
  let s = seed >>> 0;
  return () => { s = (s * 1664525 + 1013904223) >>> 0; return s / 4294967296; };
}

// ---------- placement (simulated annealing) ----------
function place(parts, nets, W, H, seed, iters) {
  const HM = Math.floor(H / 2);
  const rand = rng(seed);
  const n = parts.length;
  const state = parts.map(() => ({ x: 0, y: 0, o: 0 }));
  const idx = {};
  parts.forEach((p, i) => { idx[p.ref] = i; });
  const occ = new Map();
  const key = (x, y) => y * 1000 + x;
  const fp = parts.map(() => null);

  const netPins = Object.entries(nets).map(([name, pins]) => ({
    name,
    pins: pins.map(s => { const [r, pn] = s.split('.'); return [idx[r], pn]; }),
    w: ['5V', 'GND'].includes(name) ? 0.12 : name === 'V1' ? 0.8 : name === 'V2' ? 0.8 : name === 'Vb' ? 0.5 : 1,
  })).filter(e => e.pins.length >= 2);
  const netsOfPart = parts.map(() => []);
  netPins.forEach((e, ni) => e.pins.forEach(([pi]) => { if (!netsOfPart[pi].includes(ni)) netsOfPart[pi].push(ni); }));

  const pinPos = (pi, pn) => fp[pi].pins[pn];
  const netCost = (e) => {
    if (e.name === '5V' || e.name === 'GND') {
      // the rails run along the top (+5 V) and bottom (GND) rows: pull their pins toward them
      let s = 0;
      for (const [pi, pn] of e.pins) {
        const y = pinPos(pi, pn)[1];
        s += BANDS ? (e.name === "5V" ? Math.min(y, H - 1 - y) : Math.abs(y - HM)) : (e.name === "5V" ? y : H - 1 - y);
      }
      return RAIL_PULL * s;
    }
    let x0 = 1e9, x1 = -1e9, y0 = 1e9, y1 = -1e9;
    for (const [pi, pn] of e.pins) {
      const [x, y] = pinPos(pi, pn);
      if (x < x0) x0 = x; if (x > x1) x1 = x; if (y < y0) y0 = y; if (y > y1) y1 = y;
    }
    return e.w * ((x1 - x0) + (y1 - y0));
  };

  const fits = (pi, f) => {
    for (const [x, y] of f.cells) {
      if (x < 0 || y < 1 || x >= (BANDS ? W - 1 : W) || y > H - 2 || (BANDS && y === HM && parts[pi].kind !== 'PAD')) return false;
      if (!inRegion(parts[pi], x, y)) return false;
      const o = occ.get(key(x, y));
      if (o !== undefined && o !== pi) return false;
    }
    return true;
  };
  const setCells = (pi, f, v) => {
    for (const [x, y] of f.cells) { if (v) occ.set(key(x, y), pi); else occ.delete(key(x, y)); }
  };
  const padEdgeX = (p) => (p.edge === 'right' ? (BANDS ? W - 2 : W - 1) : 0);
  // py: row number, negative = from the bottom, or 'mid', 'mid-2', 'mid+1' relative to the middle row
  const fixedPadY = (p) => {
    if (p.py === undefined) return null;
    if (typeof p.py === 'string') return HM + Number(p.py.slice(3) || 0);
    return p.py < 0 ? H + p.py : p.py;
  };
  // region: { x: [a, b], y: [a, b] } as fractions of the board; every cell of the part must be inside
  const inRegion = (p, x, y) => {
    if (!p.region) return true;
    const [xa, xb] = p.region.x || [0, 1], [ya, yb] = p.region.y || [0, 1];
    return x >= Math.floor(xa * (W - 1)) && x <= Math.ceil(xb * (W - 1)) && y >= Math.floor(ya * (H - 1)) && y <= Math.ceil(yb * (H - 1));
  };

  // pairwise placement terms: 'near' pulls (symmetric) and noisy/sensitive separation
  const pairTerms = parts.map(() => []);
  parts.forEach((p, i) => {
    for (const [ref, w] of (p.near || [])) {
      const j = idx[ref];
      if (j === undefined) continue;
      pairTerms[i].push({ j, near: w }); pairTerms[j].push({ j: i, near: w });
    }
  });
  parts.forEach((p, i) => {
    if (!p.noisy) return;
    parts.forEach((q, j) => { if (q.sensitive) { pairTerms[i].push({ j, sep: true }); pairTerms[j].push({ j: i, sep: true }); } });
  });
  const centre = (pi) => {
    const c = fp[pi].cells; let sx = 0, sy = 0;
    for (const [x, y] of c) { sx += x; sy += y; }
    return [sx / c.length, sy / c.length];
  };
  const partCost = (pi) => {
    let s = 0;
    const [ax, ay] = centre(pi);
    for (const t of pairTerms[pi]) {
      const [bx, by] = centre(t.j);
      const d = Math.abs(ax - bx) + Math.abs(ay - by);
      if (t.sep) s += SEP_W * Math.max(0, SEP_D - d);
      else s += NEAR_W * t.near * d;
    }
    return s;
  };

  // initial: pads on edges, then everything else at random free spots
  const order = parts.map((_, i) => i).sort((a, b) => {
    // pads with a fixed row go first so no part lands on their hole
    const rank = (p) => (p.kind === 'PAD' && p.py !== undefined ? -1 : p.kind === 'INA' ? 0 : p.kind === 'DIP8' ? 1 : p.kind === 'PAD' ? 3 : 2);
    return rank(parts[a]) - rank(parts[b]);
  });
  for (const pi of order) {
    const p = parts[pi];
    let ok = false;
    for (let t = 0; t < 4000 && !ok; t++) {
      const o = (SPAN[p.kind] || p.kind === "INA" || p.kind === "DIP8") ? Math.floor(rand() * 4) : 0;
      const rx = (p.region && p.region.x) || [0, 1], ry = (p.region && p.region.y) || [0, 1];
      const x = p.kind === 'PAD' ? padEdgeX(p) : Math.floor((rx[0] + rand() * (rx[1] - rx[0])) * W);
      const fy = p.kind === 'PAD' ? fixedPadY(p) : null;
      const y = fy !== null ? fy : Math.floor((ry[0] + rand() * (ry[1] - ry[0])) * H);
      const f = footprint(p, x, y, o);
      if (fits(pi, f)) { state[pi] = { x, y, o }; fp[pi] = f; setCells(pi, f, true); ok = true; }
    }
    if (!ok) return null;
  }
  // ---- crossing estimate: each net as an MST of L-shaped wires, rails as vertical stubs to their bus ----
  const CROSS_W = parseFloat(process.env.CROSS_W || '6');
  const segsByNet = netPins.map(() => []);
  const buildSegs = (ni) => {
    const e = netPins[ni];
    const pts = e.pins.map(([pi, pn]) => pinPos(pi, pn));
    const segs = [];
    const addL = (a, b) => {
      if (a[0] !== b[0]) segs.push({ net: ni, h: true, y: a[1], a: Math.min(a[0], b[0]), b: Math.max(a[0], b[0]) });
      if (a[1] !== b[1]) segs.push({ net: ni, h: false, x: b[0], a: Math.min(a[1], b[1]), b: Math.max(a[1], b[1]) });
    };
    if (e.name === '5V' || e.name === 'GND') {
      for (const [x, y] of pts) {
        const by = BANDS ? (e.name === '5V' ? (y < HM ? 0 : H - 1) : HM) : (e.name === '5V' ? 0 : H - 1);
        if (y !== by) segs.push({ net: ni, h: false, x, a: Math.min(y, by), b: Math.max(y, by) });
      }
    } else {
      const m = pts.length;
      const used = Array(m).fill(false);
      used[0] = true;
      const dist = pts.map(p => Math.abs(p[0] - pts[0][0]) + Math.abs(p[1] - pts[0][1]));
      const from = Array(m).fill(0);
      for (let c = 1; c < m; c++) {
        let b = -1;
        for (let i = 0; i < m; i++) if (!used[i] && (b < 0 || dist[i] < dist[b])) b = i;
        used[b] = true;
        addL(pts[from[b]], pts[b]);
        for (let i = 0; i < m; i++) {
          if (used[i]) continue;
          const d = Math.abs(pts[i][0] - pts[b][0]) + Math.abs(pts[i][1] - pts[b][1]);
          if (d < dist[i]) { dist[i] = d; from[i] = b; }
        }
      }
    }
    segsByNet[ni] = segs;
  };
  const countCross = () => {
    const Hs = [], Vs = [];
    for (const list of segsByNet) for (const s of list) (s.h ? Hs : Vs).push(s);
    let c = 0;
    for (const hs of Hs) {
      for (const vs of Vs) {
        if (hs.net !== vs.net && vs.x > hs.a && vs.x < hs.b && hs.y > vs.a && hs.y < vs.b) c++;
      }
    }
    return c;
  };
  let cost = netPins.reduce((s, e) => s + netCost(e), 0);
  for (let ni = 0; ni < netPins.length; ni++) buildSegs(ni);
  let crossCur = countCross();

  for (let it = 0; it < iters; it++) {
    const T = 5 * Math.pow(0.01, it / iters);
    const pi = Math.floor(rand() * n);
    const p = parts[pi];
    const cur = state[pi];
    let nx = cur.x, ny = cur.y, no = cur.o;
    if (p.kind === 'PAD') {
      if (fixedPadY(p) !== null) continue;
      ny = cur.y + Math.floor(rand() * 9) - 4;
    } else if ((SPAN[p.kind] || p.kind === "INA" || p.kind === "DIP8") && rand() < 0.2) {
      no = (cur.o + 1 + Math.floor(rand() * 3)) % 4;
    } else {
      nx = cur.x + Math.floor(rand() * 9) - 4;
      ny = cur.y + Math.floor(rand() * 9) - 4;
    }
    if (nx === cur.x && ny === cur.y && no === cur.o) continue;
    const nf = footprint(p, nx, ny, no);
    setCells(pi, fp[pi], false);
    if (!fits(pi, nf)) { setCells(pi, fp[pi], true); continue; }
    const oldF = fp[pi];
    let before = 0;
    for (const ni of netsOfPart[pi]) before += netCost(netPins[ni]);
    before += partCost(pi);
    fp[pi] = nf;
    let after = 0;
    for (const ni of netsOfPart[pi]) after += netCost(netPins[ni]);
    after += partCost(pi);
    const oldSegs = netsOfPart[pi].map(ni => segsByNet[ni]);
    for (const ni of netsOfPart[pi]) buildSegs(ni);
    const crossNew = countCross();
    const d = (after - before) + CROSS_W * (crossNew - crossCur);
    if (d <= 0 || rand() < Math.exp(-d / T)) {
      setCells(pi, nf, true);
      state[pi] = { x: nx, y: ny, o: no };
      cost += after - before;
      crossCur = crossNew;
    } else {
      fp[pi] = oldF;
      setCells(pi, oldF, true);
      netsOfPart[pi].forEach((ni, k) => { segsByNet[ni] = oldSegs[k]; });
    }
  }
  return { state, fp, cost, cross: crossCur };
}

// ---------- router ----------
function route(parts, nets, fp, W, H, seed) {
  const rand = rng(seed);
  const idx = {};
  parts.forEach((p, i) => { idx[p.ref] = i; });
  const pinAt = new Map();
  const nodeKey = (x, y) => y * W + x;
  const pinsOfNet = {};
  for (const [name, list] of Object.entries(nets)) {
    pinsOfNet[name] = list.map(s => { const [r, pn] = s.split('.'); return fp[idx[r]].pins[pn]; });
    for (const [x, y] of pinsOfNet[name]) pinAt.set(nodeKey(x, y), name);
  }
  // pins with no net (unused adapter pins) are blocked
  parts.forEach((p, i) => {
    for (const pn of Object.keys(fp[i].pins)) {
      const [x, y] = fp[i].pins[pn];
      if (!pinAt.has(nodeKey(x, y))) pinAt.set(nodeKey(x, y), '__nc');
    }
  });

  const names = Object.keys(nets).filter(n => pinsOfNet[n].length >= 2);
  const fanout = (n) => pinsOfNet[n].length;
  let best = null;
  const prio = {};
  names.forEach(n => { prio[n] = 0; });
  const ATTEMPTS = parseInt(process.env.ATTEMPTS || "30", 10);
  for (let attempt = 0; attempt < ATTEMPTS; attempt++) {
    const key = {};
    for (const n of names) {
      const base = ["GND", "5V", "V1", "V2", "Vb"].includes(n) ? 1000 + fanout(n) : fanout(n);
      key[n] = base - 1100 * prio[n] + (attempt ? rand() * 5 : 0);
    }
    const orderN = names.slice().sort((a, b) => key[a] - key[b]);
    const res = tryRoute(orderN);
    res.order = orderN;
    if (!best || res.failed.length < best.failed.length || (res.failed.length === best.failed.length && res.hops.length < best.hops.length)) best = res;
    if (res.failed.length === 0 && attempt >= 6) break;
    for (const f of res.failed) prio[f] += 1; // a net that failed routes earlier next time
  }
  // local search over the routing order: moving one net elsewhere in the order often saves jumpers
  const LS = parseInt(process.env.LS || '150', 10);
  const scoreOf = (r) => r.failed.length * 1000 + r.hops.length;
  let curBest = best;
  for (let it = 0; it < LS; it++) {
    const order = curBest.order.slice();
    const i = Math.floor(rand() * order.length);
    const moved = order.splice(i, 1)[0];
    order.splice(Math.floor(rand() * (order.length + 1)), 0, moved);
    const res = tryRoute(order);
    res.order = order;
    if (scoreOf(res) <= scoreOf(curBest)) curBest = res;
  }
  best = curBest;
  if (best.failed.length === 0 && IMPROVE_PASSES > 0) best = improve(best);
  return best;

  function tryRoute(orderN) {
    const occ = new Map(); // nodeKey -> [{net, mask}]
    const hops = [];
    const edges = []; // {net, a:[x,y], b:[x,y]}
    const failed = [];
    // band layout: +5 V along the top and bottom rows (joined down the right edge), GND along the middle row
    const HM = Math.floor(H / 2);
    const trunkNodes = {};
    const addSeg = (name, x1, y1, x2, y2) => {
      // a straight run of bus wire from (x1,y1) to (x2,y2), either horizontal or vertical
      const dx = Math.sign(x2 - x1), dy = Math.sign(y2 - y1);
      const dirF = dx === 1 ? 0 : dx === -1 ? 2 : dy === 1 ? 1 : 3;
      const steps = Math.abs(x2 - x1) + Math.abs(y2 - y1);
      if (!trunkNodes[name]) trunkNodes[name] = new Set();
      for (let t = 0; t <= steps; t++) {
        const x = x1 + dx * t, y = y1 + dy * t;
        const k = nodeKey(x, y);
        trunkNodes[name].add(k);
        let list = occ.get(k);
        if (!list) { list = []; occ.set(k, list); }
        let e = list.find(q => q.net === name);
        if (!e) { e = { net: name, mask: 0 }; list.push(e); }
        if (t > 0) e.mask |= 1 << ((dirF + 2) % 4);
        if (t < steps) e.mask |= 1 << dirF;
      }
      for (let t = 0; t < steps; t++) {
        edges.push({ net: name, a: [x1 + dx * t, y1 + dy * t], b: [x1 + dx * (t + 1), y1 + dy * (t + 1)], trunk: true });
      }
    };
    if (BANDS) {
      if (pinsOfNet['5V']) {
        addSeg('5V', 0, 0, W - 1, 0);
        addSeg('5V', W - 1, 0, W - 1, H - 1);
        addSeg('5V', W - 1, H - 1, 0, H - 1);
      }
      if (pinsOfNet['GND']) addSeg('GND', 0, HM, W - 2, HM);
    } else {
      // single buses: +5 V along the top row, GND along the bottom row
      for (const [name, row] of [['5V', 0], ['GND', H - 1]]) {
        if (!pinsOfNet[name]) continue;
        const xs = pinsOfNet[name].map(p => p[0]);
        addSeg(name, Math.min(...xs), row, Math.max(...xs), row);
      }
    }
    for (const name of orderN) {
      const pins = pinsOfNet[name];
      const treeNodes = trunkNodes[name] ? new Set(trunkNodes[name]) : new Set([nodeKey(...pins[0])]);
      const remaining = new Set((trunkNodes[name] ? pins : pins.slice(1)).map(p => nodeKey(...p)));
      let ok = true;
      while (remaining.size > 0) {
        const path = dijkstra(name, treeNodes, remaining, occ);
        if (!path) { ok = false; break; }
        applyPath(name, path, occ, edges, hops, treeNodes);
        const endK = nodeKey(...path.nodes[path.nodes.length - 1]);
        remaining.delete(endK);
      }
      if (!ok) failed.push(name);
    }
    return { edges, hops, failed, occ, pinAt, pinsOfNet };
  }

  // ---- clean-up: rip up one net at a time, re-route it around everything else, keep it if it needs fewer jumpers ----
  function improve(start) {
    const RAILS = ['GND', '5V'];
    let edges = start.edges.slice();
    const dirOf = (a, b) => DIRS.findIndex(q => q[0] === Math.sign(b[0] - a[0]) && q[1] === Math.sign(b[1] - a[1]));
    const hopCount = (list) => list.filter(e => e.hop).length;
    const buildOcc = (list) => {
      const occ = new Map();
      const add = (net, x, y, mask, hop) => {
        const k = nodeKey(x, y);
        let l = occ.get(k);
        if (!l) { l = []; occ.set(k, l); }
        if (hop) { l.push({ net, mask, hop: true }); return; }
        let e = l.find(q => q.net === net && !q.hop);
        if (!e) { e = { net, mask: 0 }; l.push(e); }
        e.mask |= mask;
      };
      for (const e of list) {
        const d = dirOf(e.a, e.b);
        add(e.net, e.a[0], e.a[1], 1 << d);
        add(e.net, e.b[0], e.b[1], 1 << ((d + 2) % 4));
        if (e.hop) for (const h of e.hop) add(e.net, h[0], h[1], (1 << d) | (1 << ((d + 2) % 4)), true);
      }
      return occ;
    };
    // turn hops that no longer cross anything back into plain wire
    const normalize = (list) => {
      const present = new Map();
      for (const e of list) {
        for (const p of [e.a, e.b]) {
          const k = nodeKey(p[0], p[1]);
          if (!present.has(k)) present.set(k, new Set());
          present.get(k).add(e.net);
        }
      }
      for (const [name, pins] of Object.entries(pinsOfNet)) for (const p of pins) {
        const k = nodeKey(p[0], p[1]);
        if (!present.has(k)) present.set(k, new Set());
        present.get(k).add(name);
      }
      const out = [];
      for (const e of list) {
        if (!e.hop) { out.push(e); continue; }
        const stillCrosses = e.hop.some(h => {
          const s = present.get(nodeKey(h[0], h[1]));
          if (!s) return false;
          for (const n of s) if (n !== e.net) return true;
          return false;
        });
        if (stillCrosses) { out.push(e); continue; }
        const d = dirOf(e.a, e.b);
        let [x, y] = e.a;
        const steps = Math.abs(e.b[0] - e.a[0]) + Math.abs(e.b[1] - e.a[1]);
        for (let t = 0; t < steps; t++) {
          const nx = x + DIRS[d][0], ny = y + DIRS[d][1];
          out.push({ net: e.net, a: [x, y], b: [nx, ny] });
          // the straightened wire now occupies these holes, so a jumper crossing it must stay a jumper
          const kk = nodeKey(nx, ny);
          if (!present.has(kk)) present.set(kk, new Set());
          present.get(kk).add(e.net);
          x = nx; y = ny;
        }
      }
      return out;
    };
    const involvement = (list) => {
      const nodeNets = new Map();
      for (const e of list) for (const p of [e.a, e.b]) {
        const k = nodeKey(p[0], p[1]);
        if (!nodeNets.has(k)) nodeNets.set(k, new Set());
        nodeNets.get(k).add(e.net);
      }
      const score = {};
      for (const e of list) if (e.hop) {
        score[e.net] = (score[e.net] || 0) + 1;
        for (const h of e.hop) for (const n of (nodeNets.get(nodeKey(h[0], h[1])) || [])) if (n !== e.net) score[n] = (score[n] || 0) + 1;
      }
      return score;
    };
    for (let pass = 0; pass < IMPROVE_PASSES; pass++) {
      const sc = involvement(edges);
      const candidates = Object.keys(sc).sort((a, b) => sc[b] - sc[a]);
      let changed = false;
      for (const name of candidates) {
        const isRail = RAILS.includes(name);
        const rest = edges.filter(e => e.net !== name || (isRail && e.trunk));
        const occ = buildOcc(rest);
        const pins = pinsOfNet[name];
        let treeNodes, remaining;
        if (isRail) {
          treeNodes = new Set();
          for (const e of rest) if (e.net === name) { treeNodes.add(nodeKey(e.a[0], e.a[1])); treeNodes.add(nodeKey(e.b[0], e.b[1])); }
          remaining = new Set(pins.map(p => nodeKey(p[0], p[1])));
        } else {
          treeNodes = new Set([nodeKey(pins[0][0], pins[0][1])]);
          remaining = new Set(pins.slice(1).map(p => nodeKey(p[0], p[1])));
        }
        const newEdges = [], newHops = [];
        let ok = true;
        while (remaining.size > 0) {
          const path = dijkstra(name, treeNodes, remaining, occ);
          if (!path) { ok = false; break; }
          applyPath(name, path, occ, newEdges, newHops, treeNodes);
          remaining.delete(nodeKey(...path.nodes[path.nodes.length - 1]));
        }
        if (!ok) continue;
        const cand = normalize(rest.concat(newEdges));
        if (hopCount(cand) < hopCount(edges)) { edges = cand; changed = true; }
      }
      if (!changed) break;
    }
    const hops = edges.filter(e => e.hop).map(e => ({ net: e.net, a: e.a, hs: e.hop, b: e.b }));
    return { edges, hops, failed: [], occ: start.occ, pinAt: start.pinAt, pinsOfNet: start.pinsOfNet };
  }
  function occFor(occ, k) { return occ.get(k) || []; }

  function dijkstra(name, treeNodes, targets, occ) {
    const NS = W * H * 5;
    const dist = new Float64Array(NS).fill(Infinity);
    const parent = new Int32Array(NS).fill(-1);
    const hopVia = new Int32Array(NS).fill(-1);
    const hopStore = [];
    // simple binary heap
    const heap = [];
    const push = (c, s) => { heap.push([c, s]); let i = heap.length - 1; while (i > 0) { const pa = (i - 1) >> 1; if (heap[pa][0] <= heap[i][0]) break; [heap[pa], heap[i]] = [heap[i], heap[pa]]; i = pa; } };
    const pop = () => { const top = heap[0]; const last = heap.pop(); if (heap.length) { heap[0] = last; let i = 0; for (;;) { let l = 2 * i + 1, r = l + 1, m = i; if (l < heap.length && heap[l][0] < heap[m][0]) m = l; if (r < heap.length && heap[r][0] < heap[m][0]) m = r; if (m === i) break; [heap[m], heap[i]] = [heap[i], heap[m]]; i = m; } } return top; };
    for (const k of treeNodes) { const s = k * 5 + 4; dist[s] = 0; push(0, s); }
    const free = (k) => {
      const pn = pinAt.get(k);
      if (pn !== undefined && pn !== name) return false;
      for (const e of occFor(occ, k)) if (e.net !== name) return false;
      return true;
    };
    while (heap.length) {
      const [c, s] = pop();
      if (c > dist[s]) continue;
      const k = Math.floor(s / 5), d = s % 5;
      if (targets.has(k)) {
        // reconstruct
        const nodes = [];
        const hopNodes = [];
        let cur = s;
        while (cur !== -1) {
          const ck = Math.floor(cur / 5);
          nodes.push([ck % W, Math.floor(ck / W)]);
          if (hopVia[cur] !== -1) hopNodes.push(...hopStore[hopVia[cur]]);
          cur = parent[cur];
        }
        nodes.reverse();
        return { nodes, hopNodes };
      }
      const x = k % W, y = Math.floor(k / W);
      for (let nd = 0; nd < 4; nd++) {
        if (d < 4 && nd === (d + 2) % 4) continue;
        const dx = DIRS[nd][0], dy = DIRS[nd][1];
        const nx = x + dx, ny = y + dy;
        if (nx < 0 || ny < 0 || nx >= W || ny >= H) continue;
        const nk = nodeKey(nx, ny);
        const turn = d < 4 && nd !== d ? 0.4 : 0;
        const pn = pinAt.get(nk);
        if (pn !== undefined && pn !== name) continue;
        const oc = occFor(occ, nk);
        let foreign = false;
        for (const e of oc) if (e.net !== name) foreign = true;
        if (!foreign) {
          const own = oc.length > 0 || pn === name || treeNodes.has(nk);
          const nc = c + 1 + turn - (own ? 0.5 : 0);
          const ns = nk * 5 + nd;
          if (nc < dist[ns]) { dist[ns] = nc; parent[ns] = s; hopVia[ns] = -1; push(nc, ns); }
        } else {
          // hop over a single straight perpendicular foreign wire
          // a jumper can arc over one or two adjacent parallel foreign wires
          const want = (nd % 2 === 0) ? 10 : 5; // horizontal move crosses a N|S wire, vertical crosses E|W
          let hx = nx, hy = ny;
          const over = [];
          for (let kk = 1; kk <= MAXHOP; kk++) {
            if (hx < 0 || hy < 0 || hx >= W || hy >= H) break;
            const hk0 = nodeKey(hx, hy);
            const oc2 = occFor(occ, hk0);
            if (oc2.length !== 1 || oc2[0].mask !== want || oc2[0].net === name || pinAt.get(hk0) !== undefined) break;
            over.push(hk0);
            const lx = hx + dx, ly = hy + dy;
            if (lx < 0 || ly < 0 || lx >= W || ly >= H) break;
            const lk = nodeKey(lx, ly);
            if (free(lk)) {
              const ownL = occFor(occ, lk).length > 0 || pinAt.get(lk) === name || treeNodes.has(lk);
              const nc = c + (kk + 1) + turn + HOPCOST + 5 * (kk - 1) - (ownL ? 0.5 : 0);
              const ns = lk * 5 + nd;
              if (nc < dist[ns]) { dist[ns] = nc; parent[ns] = s; hopVia[ns] = hopStore.length; hopStore.push(over.slice()); push(nc, ns); }
            }
            hx = lx; hy = ly;
          }
        }
      }
    }
    return null;
  }

  function applyPath(name, path, occ, edges, hops, treeNodes) {
    const addMask = (x, y, bit) => {
      const k = nodeKey(x, y);
      let list = occ.get(k);
      if (!list) { list = []; occ.set(k, list); }
      let e = list.find(q => q.net === name);
      if (!e) { e = { net: name, mask: 0 }; list.push(e); }
      e.mask |= bit;
      treeNodes.add(k);
    };
    const hopSet = new Set(path.hopNodes);
    for (let i = 0; i + 1 < path.nodes.length; i++) {
      const [ax, ay] = path.nodes[i], [bx, by] = path.nodes[i + 1];
      const adx = Math.sign(bx - ax), ady = Math.sign(by - ay);
      const dist = Math.abs(bx - ax) + Math.abs(by - ay);
      const dir = DIRS.findIndex(q => q[0] === adx && q[1] === ady);
      if (dist >= 2) {
        // hop: a -> (one or two foreign wires) -> b
        addMask(ax, ay, 1 << dir);
        addMask(bx, by, 1 << ((dir + 2) % 4));
        const hs = [];
        for (let t = 1; t < dist; t++) {
          const hx = ax + adx * t, hy = ay + ady * t;
          const hk = nodeKey(hx, hy);
          occ.get(hk).push({ net: name, mask: (1 << dir) | (1 << ((dir + 2) % 4)), hop: true });
          hs.push([hx, hy]);
        }
        hops.push({ net: name, a: [ax, ay], hs, b: [bx, by] });
        edges.push({ net: name, a: [ax, ay], b: [bx, by], hop: hs });
      } else {
        addMask(ax, ay, 1 << dir);
        addMask(bx, by, 1 << ((dir + 2) % 4));
        edges.push({ net: name, a: [ax, ay], b: [bx, by] });
      }
    }
    const [lx, ly] = path.nodes[path.nodes.length - 1];
    treeNodes.add(nodeKey(lx, ly));
  }
}

module.exports = { footprint, place, route, SPAN, DIRS };
