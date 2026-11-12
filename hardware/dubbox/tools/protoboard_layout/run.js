const fs = require('fs');
const { build } = require('./netlist');
const { footprint, place, route, SPAN, DIRS } = require('./layout');

function verify(parts, nets, fp, result, W, H) {
  const idx = {};
  parts.forEach((p, i) => { idx[p.ref] = i; });
  const problems = [];
  // union find over node keys, per net
  const key = (x, y) => y * W + x;
  const parentOf = new Map();
  const find = (a) => { while (parentOf.get(a) !== a) { parentOf.set(a, parentOf.get(parentOf.get(a))); a = parentOf.get(a); } return a; };
  const union = (a, b) => { if (!parentOf.has(a)) parentOf.set(a, a); if (!parentOf.has(b)) parentOf.set(b, b); parentOf.set(find(a), find(b)); };
  const nodeNets = new Map();
  const touch = (x, y, net) => {
    const k = key(x, y);
    if (!nodeNets.has(k)) nodeNets.set(k, new Set());
    nodeNets.get(k).add(net);
    if (!parentOf.has(k)) parentOf.set(k, k);
  };
  for (const e of result.edges) {
    touch(e.a[0], e.a[1], e.net); touch(e.b[0], e.b[1], e.net);
    union(key(...e.a), key(...e.b));
  }
  // pins of every net count as touching their node
  const pinNodeNet = new Map();
  for (const [name, list] of Object.entries(nets)) {
    for (const s of list) {
      const [r, pn] = s.split('.');
      const [x, y] = fp[idx[r]].pins[pn];
      const k = key(x, y);
      if (pinNodeNet.has(k) && pinNodeNet.get(k) !== name) problems.push(`two nets on one hole at ${x},${y}: ${pinNodeNet.get(k)} and ${name}`);
      pinNodeNet.set(k, name);
      touch(x, y, name);
    }
  }
  for (const [k, set] of nodeNets) {
    if (set.size > 1) problems.push(`node ${k % W},${Math.floor(k / W)} shared by ${[...set].join(' / ')}`);
  }
  // hop nodes: a foreign straight wire must be there and no pin
  for (const h of result.hops) {
    for (const hh of h.hs) { if (pinNodeNet.has(key(...hh))) problems.push(`hop over a pin at ${hh}`); }
  }
  // connectivity
  for (const [name, list] of Object.entries(nets)) {
    if (list.length < 2) continue;
    const roots = new Set();
    for (const s of list) {
      const [r, pn] = s.split('.');
      const [x, y] = fp[idx[r]].pins[pn];
      roots.add(find(key(x, y)));
    }
    if (roots.size !== 1) problems.push(`net ${name} is not fully connected (${roots.size} pieces)`);
  }
  // different nets must not share a component (short)
  const compNet = new Map();
  for (const [k] of parentOf) {
    const r = find(k);
    for (const n of (nodeNets.get(k) || [])) {
      if (compNet.has(r) && compNet.get(r) !== n) problems.push(`short between ${compNet.get(r)} and ${n}`);
      compNet.set(r, n);
    }
  }
  return problems;
}

function attempt(W, H, seeds, iters) {
  const { parts, nets } = build();
  let best = null;
  for (const seed of seeds) {
    const pl = place(parts, nets, W, H, seed, iters);
    if (!pl) continue;
    const r = route(parts, nets, pl.fp, W, H, seed + 7);
    const score = r.failed.length * 1000 + r.hops.length * 4 + pl.cost * 0.05;
    if (!best || score < best.score) best = { parts, nets, pl, r, score, seed };
    if (r.failed.length === 0 && best.score < 400) { /* keep searching a bit for fewer hops */ }
  }
  return best;
}

module.exports = { attempt, verify };

if (require.main === module) {
  const W = parseInt(process.argv[2], 10), H = parseInt(process.argv[3], 10);
  const nseeds = parseInt(process.argv[4] || '6', 10);
  const iters = parseInt(process.argv[5] || '120000', 10);
  const seeds = Array.from({ length: nseeds }, (_, i) => parseInt(process.env.SEEDBASE || '101', 10) + i * 37);
  const b = attempt(W, H, seeds, iters);
  if (!b) { console.log('no placement'); process.exit(1); }
  const probs = verify(b.parts, b.nets, b.pl.fp, b.r, W, H);
  console.log(`W=${W} H=${H} seed=${b.seed} failedNets=${b.r.failed.length} hops=${b.r.hops.length} cost=${b.pl.cost.toFixed(0)} estCross=${b.pl.cross} problems=${probs.length}`);
  if (b.r.failed.length) console.log('failed: ' + b.r.failed.join(', '));
  probs.slice(0, 8).forEach(p => console.log('  ' + p));
  fs.writeFileSync(__dirname + `/result_${W}x${H}_${process.env.TAG || 'x'}.json`, JSON.stringify({ W, H, seed: b.seed, state: b.pl.state, edges: b.r.edges, hops: b.r.hops, failed: b.r.failed }));
}
