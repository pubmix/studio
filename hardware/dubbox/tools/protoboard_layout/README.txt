Protoboard layout generator for the Dub-Box input board (both channels, no phantom power).

netlist.js  parts, nets and the floorplan (regions, fixed pad rows, noisy/sensitive tags). Edit this if the circuit changes.
layout.js   footprints, placement (simulated annealing with a crossing estimate + analog rules), router, jumper clean-up
run.js      node run.js <cols> <rows> <seeds> <iterations>   (writes result_<cols>x<rows>_<TAG>.json)
render.js   node render.js <result.json> <out.svg>

best_final.json  rev 2, 44 x 28 holes, 12 jumpers  -> DUBBOX_INPUT_PROTOBOARD_LAYOUT.svg
alt_40x26.json   rev 2, 40 x 26 holes, 17 jumpers  -> DUBBOX_INPUT_PROTOBOARD_LAYOUT_40x26.svg

Rev 2 (2026-11-04): band rails (+5 V on top/bottom rows and right column, GND on the middle row), channel 2 in the
top half and channel 1 in the bottom half, each charge pump in its own outer right corner with its own ground pad,
RC filter after each pump, single 110 ohm gain resistor. Best settings found:
  SEP_W=0.5 SEP_D=6 NEAR_W=0.5 HOPCOST=16 SEEDBASE=22011 node run.js 44 28 6 300000
Searches are random: run several SEEDBASE values in parallel and keep the one with fewest hops and no failed nets.

The environment variables CROSS_W, HOPCOST, RAIL_PULL, IMPROVE, SEEDBASE, SEP_D, SEP_W, NEAR_W and BANDS (0 = old
top/bottom rails) tune the search.
