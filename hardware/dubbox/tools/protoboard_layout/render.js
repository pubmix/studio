const fs = require('fs');
const { build } = require('./netlist');
const { footprint, SPAN, DIRS } = require('./layout');

const file = process.argv[2] || (__dirname + '/best.json');
const outFile = process.argv[3] || (__dirname + '/board.svg');
const rec = JSON.parse(fs.readFileSync(file, 'utf8'));
const W = rec.W, H = rec.H;
const { parts, nets } = build();
const fp = parts.map((p, i) => footprint(p, rec.state[i].x, rec.state[i].y, rec.state[i].o));
const hopsList = rec.hopsList || rec.hops || [];

const S = 18;
const ML = 200, MR = 200, MT = 92, MB = 56;
const bx = ML, by = MT;
const bw = (W - 1) * S, bh = (H - 1) * S;
const X = (x) => bx + x * S;
const Y = (y) => by + y * S;
const PAD = 14; // board border around the outermost holes

const out = [];
const esc = (s) => String(s).replace(/&/g, '&amp;').replace(/</g, '&lt;');
const text = (x, y, s, o = {}) => out.push(`<text x="${x}" y="${y}" font-size="${o.size || 10}" fill="${o.fill || '#2c2c2a'}"${o.anchor ? ` text-anchor="${o.anchor}"` : ''}${o.weight ? ` font-weight="${o.weight}"` : ''} dominant-baseline="central">${esc(s)}</text>`);

// parts table layout (below the board)
const rowsPerCol = 22;
const sorted = parts.slice().sort((a, b) => a.ref.localeCompare(b.ref, undefined, { numeric: true }));
const cols = Math.ceil(sorted.length / rowsPerCol);
const tableTop = by + bh + MB + 30;
const totalW = ML + bw + MR;
const totalH = tableTop + rowsPerCol * 14 + 190;

out.push(`<svg xmlns="http://www.w3.org/2000/svg" width="100%" viewBox="0 0 ${totalW} ${totalH}" role="img" font-family="Arial,Helvetica,sans-serif">`);
out.push('<title>Dub-Box input board: single-layer protoboard layout</title>');
out.push(`<desc>Perfboard layout of the whole two-channel input circuit on a ${W} by ${H} hole board. Silver lines are solder bridges or wires; wires drawn with an arc hop over another wire and must be insulated.</desc>`);
out.push('<rect width="100%" height="100%" fill="#ffffff"/>');
text(14, 18, 'Dub-Box input board: both channels on one protoboard', { size: 16, weight: 700 });
text(14, 38, `${W} × ${H} holes (${((W - 1) * 2.54 + 2 * 3).toFixed(0)} × ${((H - 1) * 2.54 + 2 * 3).toFixed(0)} mm), 0.1 in pitch, components on top, silver lines are the wiring underneath`, { size: 11, fill: '#555' });

// board
out.push(`<rect x="${bx - PAD}" y="${by - PAD}" width="${bw + 2 * PAD}" height="${bh + 2 * PAD}" rx="6" fill="#3a7d5c" stroke="#1d4d37" stroke-width="2"/>`);
// holes
for (let y = 0; y < H; y++) for (let x = 0; x < W; x++) {
  out.push(`<circle cx="${X(x)}" cy="${Y(y)}" r="3.3" fill="#c79a4a" opacity="0.55"/><circle cx="${X(x)}" cy="${Y(y)}" r="1.3" fill="#14301f"/>`);
}
// column ticks
for (let x = 0; x < W; x += 5) {
  text(X(x), by - PAD - 8, String(x), { size: 8, fill: '#666', anchor: 'middle' });
  text(X(x), by + bh + PAD + 10, String(x), { size: 8, fill: '#666', anchor: 'middle' });
}
for (let y = 0; y < H; y += 5) text(14, Y(y), String(y), { size: 8, fill: '#666' });

// component bodies
const COL = { R: ['#dcc29a', '#8a6a3a'], D: ['#2e2e33', '#111'], C: ['#4a7bd0', '#274a8a'], E: ['#3a4660', '#1c2438'] };
parts.forEach((p, i) => {
  const f = fp[i];
  if (SPAN[p.kind]) {
    const names = Object.keys(f.pins);
    const [x1, y1] = f.pins[names[0]], [x2, y2] = f.pins[names[1]];
    const cx = (X(x1) + X(x2)) / 2, cy = (Y(y1) + Y(y2)) / 2;
    const horiz = y1 === y2;
    const [fill, stroke] = COL[p.kind];
    if (p.kind === 'E') {
      out.push(`<circle cx="${cx}" cy="${cy}" r="${1.1 * S}" fill="${fill}" stroke="${stroke}" stroke-width="1.2"/>`);
      const px = X(x1) + (X(x2) - X(x1)) * 0.0, py = Y(y1);
      out.push(`<path d="M ${cx - 0.55 * S} ${cy - 0.9 * S} A ${1.05 * S} ${1.05 * S} 0 0 0 ${cx - 0.55 * S} ${cy + 0.9 * S} L ${cx - 0.15 * S} ${cy + 0.9 * S} A ${1.05 * S} ${1.05 * S} 0 0 1 ${cx - 0.15 * S} ${cy - 0.9 * S} Z" fill="#9aa4b8" opacity="0.35" transform="rotate(${horiz ? 0 : 90} ${cx} ${cy})"/>`);
      // + mark near the + pin
      const mx = X(x1) + (cx - X(x1)) * 0.45, my = Y(y1) + (cy - Y(y1)) * 0.45;
      text(mx + (horiz ? 0 : -9), my + (horiz ? -9 : 0), '+', { size: 11, fill: '#ffd24d', anchor: 'middle', weight: 700 });
      text(cx, cy + (horiz ? 0.55 * S + 4 : 0), p.ref, { size: 8, fill: '#e7edf7', anchor: 'middle' });
    } else {
      const len = (SPAN[p.kind] - (p.kind === 'D' ? 1.2 : p.kind === 'R' ? 1.3 : 1.0)) * S;
      const th = (p.kind === 'R' ? 0.8 : p.kind === 'D' ? 0.7 : 0.9) * S;
      const w = horiz ? len : th, h = horiz ? th : len;
      out.push(`<rect x="${cx - w / 2}" y="${cy - h / 2}" width="${w}" height="${h}" rx="${p.kind === 'C' ? 2 : 4}" fill="${fill}" stroke="${stroke}" stroke-width="1"/>`);
      if (p.kind === 'D') {
        // silver band at the cathode (pin K) end
        const [kx, ky] = f.pins.K;
        const t = 0.32 * S;
        const dx = Math.sign(kx - cx), dy = Math.sign(ky - cy);
        const bxx = cx + dx * (len / 2 - t / 2), byy = cy + dy * (len / 2 - t / 2);
        const bwid = horiz ? t : th, bhei = horiz ? th : t;
        out.push(`<rect x="${bxx - bwid / 2}" y="${byy - bhei / 2}" width="${bwid}" height="${bhei}" fill="#cfd4dc"/>`);
      }
      text(cx, cy + (horiz ? -th / 2 - 5 : 0), p.ref, { size: 7.5, fill: p.kind === 'D' ? '#e6e6e6' : '#1a1a1a', anchor: 'middle' });
    }
  } else if (p.kind === 'INA' || p.kind === 'DIP8') {
    // body from the real pin positions, so rotated parts are drawn where they actually sit
    const pts = Object.values(f.pins);
    const xs = pts.map(q => q[0]), ys = pts.map(q => q[1]);
    const xa = Math.min(...xs), xb = Math.max(...xs), ya = Math.min(...ys), yb = Math.max(...ys);
    const vertical = (xb - xa) === 3; // pin rows run up/down
    out.push(`<rect x="${X(xa) - 0.55 * S}" y="${Y(ya) - 0.55 * S}" width="${(xb - xa) * S + 1.1 * S}" height="${(yb - ya) * S + 1.1 * S}" rx="5" fill="#212126" stroke="#58585f" stroke-width="1.2"/>`);
    const [p1x, p1y] = f.pins['1'];
    const ccx = (X(xa) + X(xb)) / 2, ccy = (Y(ya) + Y(yb)) / 2;
    // pin-1 dot just inside the body next to pin 1
    out.push(`<circle cx="${X(p1x) + Math.sign(ccx - X(p1x)) * 0.45 * S}" cy="${Y(p1y) + Math.sign(ccy - Y(p1y)) * 0.45 * S}" r="3" fill="#ffd24d"/>`);
    const label = p.value + (p.desc.startsWith('ch') ? ' ' + p.desc.slice(0, 3) : '');
    if (vertical) out.push(`<g transform="rotate(-90 ${ccx} ${ccy})">`);
    text(ccx, ccy, label + ' (' + p.ref + ')', { size: 10, fill: '#e8e8ee', anchor: 'middle', weight: 700 });
    if (vertical) out.push('</g>');
    for (const [pn, [px, py]] of Object.entries(f.pins)) {
      // pin number just inside the body
      const ix = vertical ? Math.sign(ccx - X(px)) * 0.62 * S : 0;
      const iy = vertical ? 0 : Math.sign(ccy - Y(py)) * 0.62 * S;
      text(X(px) + ix, Y(py) + iy, pn, { size: 6.5, fill: '#b9b9c2', anchor: vertical ? (ix > 0 ? 'start' : 'end') : 'middle' });
    }
  }
});

// wires (silver). Hops are drawn as arcs and mark insulated jumpers.
const wireOutline = [], wireInner = [], hopOutline = [], hopInner = [];
for (const e of rec.edges) {
  const [ax, ay] = e.a, [bx2, by2] = e.b;
  if (e.hop) {
    const dx = Math.sign(bx2 - ax), dy = Math.sign(by2 - ay);
    const hx = e.hop[0], hy = e.hop[1];
    const pxo = -dy, pyo = dx; // perpendicular
    const s1x = X(hx) - dx * 0.45 * S, s1y = Y(hy) - dy * 0.45 * S;
    const s2x = X(hx) + dx * 0.45 * S, s2y = Y(hy) + dy * 0.45 * S;
    const cxq = X(hx) + pxo * 1.0 * S, cyq = Y(hy) + pyo * 1.0 * S;
    const d = `M ${X(ax)} ${Y(ay)} L ${s1x} ${s1y} Q ${cxq} ${cyq} ${s2x} ${s2y} L ${X(bx2)} ${Y(by2)}`;
    hopOutline.push(`<path d="${d}" fill="none" stroke="#1d4ed8" stroke-width="6.6" stroke-linecap="round" stroke-linejoin="round"/>`);
    hopInner.push(`<path d="${d}" fill="none" stroke="#dbe7ff" stroke-width="4.2" stroke-linecap="round" stroke-linejoin="round"/>`);
  } else {
    const d = `M ${X(ax)} ${Y(ay)} L ${X(bx2)} ${Y(by2)}`;
    wireOutline.push(`<path d="${d}" stroke="#4b5563" stroke-width="6.4" stroke-linecap="round"/>`);
    wireInner.push(`<path d="${d}" stroke="#dde2ea" stroke-width="4.2" stroke-linecap="round"/>`);
  }
}
out.push(...wireOutline, ...wireInner, ...hopOutline, ...hopInner);

// pads / lead holes on top
parts.forEach((p, i) => {
  for (const [pn, [px, py]] of Object.entries(fp[i].pins)) {
    const isPad = p.kind === 'PAD';
    out.push(`<circle cx="${X(px)}" cy="${Y(py)}" r="${isPad ? 5.6 : 4.4}" fill="${isPad ? '#e8c35c' : '#d9d9de'}" stroke="#4b3a12" stroke-width="1"/><circle cx="${X(px)}" cy="${Y(py)}" r="1.5" fill="#14301f"/>`);
  }
});

// pad labels
const padLabel = (p) => {
  const m = p.ref.match(/^J(\d)-(.+)$/);
  if (m) {
    const nice = { XLR3: 'XLR 3 (cold)', XLR2: 'XLR 2 (hot)', T: '1/4" T (tip)', GND: 'jack GND (XLR 1, G, TS, S)', OUT: p.value };
    return `Ch${m[1]} ${nice[m[2]] || m[2]}`;
  }
  const pm = p.ref.match(/^PGND-P(\d)$/);
  if (pm) return `Pump ${pm[1]} GND (own wire to star)`;
  return p.value === 'GND' ? 'Analog GND (star)' : 'Power +5 V';
};
parts.forEach((p, i) => {
  if (p.kind !== 'PAD') return;
  const [px, py] = fp[i].pins.P;
  if (p.edge === 'right') text(X(W - 1) + 14, Y(py), padLabel(p), { size: 10, weight: 700 });
  else text(X(px) - 14, Y(py), padLabel(p), { size: 10, weight: 700, anchor: 'end' });
});

// bus labels
const bands = rec.edges.some(e => e.trunk && e.net === 'GND' && e.a[1] === Math.floor(H / 2) && e.b[1] === Math.floor(H / 2));
if (bands) {
  const HM = Math.floor(H / 2);
  text(bx + bw / 2, by - PAD - 26, '+5 V bus (top row, right column, bottom row)', { size: 11, weight: 700, fill: '#b3261e', anchor: 'middle' });
  text(bx + bw / 2, by + bh + PAD + 26, '+5 V bus (bottom row)  ·  GND bus = middle row ' + HM + '  ·  top half = channel 2, bottom half = channel 1', { size: 11, weight: 700, fill: '#333', anchor: 'middle' });
  out.push(`<path d="M ${X(0) - 10} ${Y(HM)} l -6 -5 v 10 z" fill="#333"/>`);
} else {
  text(bx + bw / 2, by - PAD - 26, '+5 V bus (top row)', { size: 11, weight: 700, fill: '#b3261e', anchor: 'middle' });
  text(bx + bw / 2, by + bh + PAD + 26, 'GND bus (bottom row)', { size: 11, weight: 700, fill: '#333', anchor: 'middle' });
}

// legend
const ly = by + bh + PAD + 44;
out.push(`<path d="M ${bx} ${ly} L ${bx + 34} ${ly}" stroke="#4b5563" stroke-width="6.4" stroke-linecap="round"/><path d="M ${bx} ${ly} L ${bx + 34} ${ly}" stroke="#dde2ea" stroke-width="4.2" stroke-linecap="round"/>`);
text(bx + 44, ly, 'silver line = solder bridge or bare wire under the board', { size: 11 });
out.push(`<path d="M ${bx + 400} ${ly} L ${bx + 412} ${ly} Q ${bx + 421} ${ly - 16} ${bx + 430} ${ly} L ${bx + 442} ${ly}" fill="none" stroke="#1d4ed8" stroke-width="6.6" stroke-linecap="round"/><path d="M ${bx + 400} ${ly} L ${bx + 412} ${ly} Q ${bx + 421} ${ly - 16} ${bx + 430} ${ly} L ${bx + 442} ${ly}" fill="none" stroke="#dbe7ff" stroke-width="4.2" stroke-linecap="round"/>`);
text(bx + 452, ly, 'arc = this wire hops over another: use insulated wire', { size: 11 });

// parts table
text(14, tableTop - 18, 'Parts and positions (column, row of each part\'s first lead; INA163 / TL072 / ICL7660S at pin 1)', { size: 12, weight: 700 });
const colW = Math.floor((totalW - 28) / cols);
sorted.forEach((p, k) => {
  const c = Math.floor(k / rowsPerCol), r = k % rowsPerCol;
  const i = parts.indexOf(p);
  const pos = `(${rec.state[i].x},${rec.state[i].y})`;
  const val = p.kind === 'PAD' ? '' : p.value;
  text(14 + c * colW, tableTop + r * 14, `${p.ref}  ${val}  ${pos}`, { size: 9.5 });
});
const noteY = tableTop + rowsPerCol * 14 + 18;
const notes = [
  'Assumptions: SOIC adapter boards (INA163 14-pin, ICL7660S 8-pin) have their two pin rows 0.3 in (3 holes) apart; TL072 is a DIP-8.',
  'Resistors are 1/4 W axial, spacing 0.4 in. 1N4148 spacing 0.3 in (silver band = cathode). Film caps 0.2 in. Electrolytics 0.2 in, longer lead is +.',
  'The two combo jacks are wired to the left-edge pads with short wires (their terminal layout is not in the datasheet). XLR 1, XLR G, TS and S all go to GND.',
  'Power: +5 V on P+5V. Run THREE separate ground wires to the star point (Audio Shield G pad): PGND (analog), PGND-P1 and PGND-P2 (charge pumps). Do not link them on the board.',
  'LINE IN L / LINE IN R go from the right-edge pads to the Audio Shield. Phantom power is not fitted.',
  'Each charge pump feeds its rail through a 10 Ω + 100 µF filter; only the filtered rail (V−1 / V−2) reaches the preamps, TL072, clamps and Vb.',
  'Gain resistor: one 110 Ω per channel (G = 1 + 6000/110 ≈ 55x for a mic). Fit it in two socket pins so it can be swapped (about 470 Ω suits an instrument).',
];
notes.forEach((n, k) => text(14, noteY + k * 16, n, { size: 10, fill: '#444' }));
out.push('</svg>');
fs.writeFileSync(outFile, out.join('\n'));
console.log(`wrote ${outFile} (${totalW}x${totalH}) hops=${rec.edges.filter(e => e.hop).length}`);
