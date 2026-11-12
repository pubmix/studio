// Netlist and parts for the Dub-Box input board (both channels, no phantom power).
// Part kinds: R (axial, span 4), D (axial, span 3), C (film, span 2), E (electrolytic, span 2),
// INA (14-pin adapter, rows 3 apart), DIP8 (8-pin, rows 3 apart), PAD (single hole).
//
// Placement hints read by layout.js:
//   PAD  edge 'left'/'right' and py = fixed row (negative = counted up from the bottom row, 'mid+2' = relative to the middle row)
//   region     { x: [a, b], y: [a, b] } fractions of the board the part must stay inside (the floorplan)
//   noisy      charge-pump parts: kept away from every 'sensitive' part
//   sensitive  small-signal parts (mic chain, gain resistor, preamp, instrument buffer)
//   near       [[ref, weight], ...] extra pull toward another part (hot/cold leg pairs)
//
// Floorplan (band layout: +5 V on the top and bottom rows and the right column, GND on the middle row):
//   top half = channel 2, bottom half = channel 1. Jacks on the left edge, mic chain left, INA163 centre,
//   line outputs on the right edge near the middle, each charge pump in its own outer right corner,
//   TL072 + Vb near the middle.
//
// Rev 2 (2026-11-04): single 110 ohm gain resistor, RC filter (10 ohm + 100 uF) after each charge pump,
// 47 uF pump output caps, a 100 uF +5 V bulk cap at each pump, and a separate ground pad per pump
// (PGND-P1, PGND-P2) so pump current does not flow along the analog ground row.

function build() {
  const parts = [];
  const nets = {};
  const counters = {};
  const nextRef = (prefix) => { counters[prefix] = (counters[prefix] || 0) + 1; return prefix + counters[prefix]; };
  const add = (prefix, kind, value, desc, extra = {}) => {
    const ref = extra.ref || nextRef(prefix);
    parts.push({ ref, kind, value, desc, ...extra });
    return ref;
  };
  const net = (name, ...pins) => { (nets[name] = nets[name] || []).push(...pins); };
  const S = { sensitive: true };
  const N = { noisy: true };
  const band = (c) => (c === 2 ? [0, 0.5] : [0.5, 1]); // channel 2 top, channel 1 bottom
  const reg = (x, y) => ({ region: { x, y } });

  // power pads (right edge; the star ground is at the Audio Shield end)
  const p5 = add('P', 'PAD', '+5 V', 'power in +5 V', { ref: 'P+5V', edge: 'right', py: 1 });
  const pg = add('P', 'PAD', 'GND', 'analog GND (own wire to the star point at the Audio Shield G pad)', { ref: 'PGND', edge: 'right', py: 'mid' });
  net('5V', `${p5}.P`);
  net('GND', `${pg}.P`);

  // shared: TL072 and the Vb bias, near the middle
  const mid = reg([0.15, 0.6], [0.25, 0.75]);
  const tl = add('U', 'DIP8', 'TL072', 'dual op amp (both halves used)', { ref: 'U3', ...S, ...mid });
  net('5V', `${tl}.8`);
  net('V1', `${tl}.4`);
  const ct1 = add('C', 'C', '0.1µF', 'TL072 +5 V decoupling', { ...mid });
  const ct2 = add('C', 'C', '0.1µF', 'TL072 V−1 decoupling', { ...mid });
  net('5V', `${ct1}.1`); net('GND', `${ct1}.2`);
  net('V1', `${ct2}.1`); net('GND', `${ct2}.2`);

  const rb1 = add('R', 'R', '100k', 'Vb divider top (+5 V to Vb)', { ...mid });
  const rb2 = add('R', 'R', '100k', 'Vb divider bottom (Vb to V−1)', { ...mid });
  const cvb = add('C', 'E', '10µF', 'Vb filter (+ leg at Vb)', { ...mid });
  net('5V', `${rb1}.1`); net('Vb', `${rb1}.2`, `${rb2}.1`, `${cvb}.+`);
  net('V1', `${rb2}.2`); net('GND', `${cvb}.-`);

  // charge pumps: V-1 (bottom right corner, feeds channel 1 and the TL072), V-2 (top right corner, channel 2)
  for (let k = 1; k <= 2; k++) {
    const corner = reg([0.72, 1], band(k));
    const gp = add('P', 'PAD', 'GND', `charge pump ${k} GND: own wire to the star point`,
      { ref: `PGND-P${k}`, edge: 'right', py: k === 1 ? -4 : 3 });
    const u = add('U', 'DIP8', 'ICL7660S', `negative rail V−${k}`, { ref: 'U' + k, ...N, ...corner });
    const c1 = add('C', 'E', '10µF', `ICL7660S #${k} pump capacitor C1 (+ leg to pin 2)`, { ...N, ...corner });
    const c2 = add('C', 'E', '47µF', `ICL7660S #${k} output capacitor C2 (− leg to raw V−${k})`, { ...N, ...corner });
    const rf = add('R', 'R', '10Ω', `V−${k} filter resistor (raw to clean rail)`, { ...N, ...corner });
    const ef = add('C', 'E', '100µF', `V−${k} filter capacitor (− leg to clean V−${k}, + leg to GND)`, { ...reg([0.55, 1], band(k)) });
    const bulk = add('C', 'E', '100µF', `+5 V bulk capacitor at pump ${k} (+ leg to +5 V)`, { ...N, ...corner });
    net('5V', `${u}.1`, `${u}.8`, `${bulk}.+`);
    net(`GNDP${k}`, `${gp}.P`, `${u}.3`, `${c2}.+`, `${bulk}.-`);
    net(`CAPP${k}`, `${u}.2`, `${c1}.+`);
    net(`CAPM${k}`, `${u}.4`, `${c1}.-`);
    net(`VR${k}`, `${u}.5`, `${c2}.-`, `${rf}.1`);
    net(`V${k}`, `${rf}.2`, `${ef}.-`);
    net('GND', `${ef}.+`);
  }

  // two channels
  for (let c = 1; c <= 2; c++) {
    const VM = 'V' + c;
    const tag = `ch${c}`;
    const yb = band(c);
    const micR = reg([0, 0.5], yb), inaR = reg([0.25, 0.8], yb), instR = reg([0, 0.6], yb);
    const ina = add('U', 'INA', 'INA163', `${tag} mic preamp`, { ref: 'U' + (3 + c), ...S, ...inaR });
    // jack pads: XLR 2 and 3 on adjacent rows so the pair runs together; jack GND next to the GND row
    const rows = c === 2 ? { T: 2, XLR2: 5, XLR3: 6, GND: 'mid-1' } : { T: -3, XLR2: -6, XLR3: -7, GND: 'mid+1' };
    const pad = (name, desc) => add('J', 'PAD', name, `${tag} ${desc}`, { ref: `J${c}-${name}`, edge: 'left', py: rows[name] });
    const j3 = pad('XLR3', 'XLR pin 3 (cold)');
    const j2 = pad('XLR2', 'XLR pin 2 (hot)');
    const jgnd = pad('GND', 'jack ground: XLR 1, XLR G, TS and S joined at the jack');
    const jt = pad('T', '1/4 in tip');
    const line = add('J', 'PAD', c === 1 ? 'LINE IN L' : 'LINE IN R', `${tag} output to the Audio Shield`,
      { ref: `J${c}-OUT`, edge: 'right', py: c === 1 ? 'mid+5' : 'mid-5' });
    net("GND", `${jgnd}.P`);

    // mic chains (hot and cold legs pulled next to each other)
    const legParts = {};
    for (const leg of ['cold', 'hot']) {
      const eC = add('C', 'E', '33µF', `${tag} ${leg} coupling (+ leg to jack)`, { ...S, ...micR });
      const r47 = add('R', 'R', '47Ω', `${tag} ${leg} input series`, { ...S, ...micR });
      const r22 = add('R', 'R', '2.2k', `${tag} ${leg} bias to GND`, { ...S, ...micR });
      const dUp = add('D', 'D', '1N4148', `${tag} ${leg} clamp to +5 V (anode at the node)`, { ...S, ...micR });
      const dDn = add('D', 'D', '1N4148', `${tag} ${leg} clamp to V−${c} (anode at the rail)`, { ...S, ...micR });
      legParts[leg] = [eC, r47, r22];
      const jpad = leg === 'cold' ? j3 : j2;
      const inaPin = leg === 'cold' ? '4' : '5';
      const nJ = `J_${leg}_${c}`, nB = `B_${leg}_${c}`, nC = `C_${leg}_${c}`;
      net(nJ, `${jpad}.P`, `${eC}.+`);
      net(nB, `${eC}.-`, `${r47}.1`, `${r22}.1`);
      net(nC, `${r47}.2`, `${dUp}.A`, `${dDn}.K`, `${ina}.${inaPin}`);
      net('GND', `${r22}.2`);
      net('5V', `${dUp}.K`);
      net(VM, `${dDn}.A`);
    }
    legParts.hot.forEach((ref, i) => {
      parts.find(p => p.ref === ref).near = [[legParts.cold[i], 0.5]];
    });

    // INA163 supply, reference, output
    const cd1 = add('C', 'C', '0.1µF', `${tag} INA163 +5 V decoupling`, { ...inaR });
    const cd2 = add('C', 'C', '0.1µF', `${tag} INA163 V−${c} decoupling`, { ...inaR });
    net('5V', `${ina}.11`, `${cd1}.1`); net('GND', `${ina}.10`, `${cd1}.2`, `${cd2}.2`);
    net(VM, `${ina}.6`, `${cd2}.1`);
    // gain resistor: one 110 ohm (G = 1 + 6000/110 = 55.5). Socket its two holes so it can be swapped.
    const rg = add('R', 'R', '110Ω', `${tag} gain resistor RG (socket it: 110 Ω mic, about 470 Ω instrument)`, { ...S, ...inaR });
    net(`RGA_${c}`, `${ina}.3`, `${rg}.1`);
    net(`RGB_${c}`, `${rg}.2`, `${ina}.12`);
    const rout = add('R', 'R', '1k', `${tag} output series`, { ...reg([0.4, 1], yb) });
    net(`VO_${c}`, `${ina}.8`, `${ina}.9`, `${rout}.1`);
    net(`LINE_${c}`, `${rout}.2`, `${line}.P`);

    // instrument path (TL072 half A for ch1, half B for ch2)
    const tp = c === 1 ? { plus: '3', minus: '2', out: '1' } : { plus: '5', minus: '6', out: '7' };
    const ri = add('R', 'R', '1k', `${tag} instrument input series`, { ...S, ...instR });
    const ci = add('C', 'C', '0.1µF', `${tag} instrument coupling`, { ...S, ...instR });
    const r1m = add('R', 'R', '1M', `${tag} instrument bias to Vb`, { ...S, ...reg([0, 0.6], [0.1, 0.9]) });
    const eo = add('C', 'E', '10µF', `${tag} buffer output coupling (+ leg to TL072)`, { ...S, ...reg([0.1, 0.7], [0.1, 0.9]) });
    const r22k = add('R', 'R', '22k', `${tag} instrument into INA163`, { ...S, ...reg([0.1, 0.8], [0.1, 0.9]) });
    net(`T_${c}`, `${jt}.P`, `${ri}.1`);
    net(`Ia_${c}`, `${ri}.2`, `${ci}.1`);
    net(`P3_${c}`, `${ci}.2`, `${tl}.${tp.plus}`, `${r1m}.1`);
    net('Vb', `${r1m}.2`);
    net(`OUT_${c}`, `${tl}.${tp.out}`, `${tl}.${tp.minus}`, `${eo}.+`);
    net(`Ib_${c}`, `${eo}.-`, `${r22k}.1`);
    net(`C_hot_${c}`, `${r22k}.2`);
  }
  return { parts, nets };
}

module.exports = { build };
