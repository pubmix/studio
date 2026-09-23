# Benchmark protocol

No representative song corpus or isolated multitracks were found in the task workspace. The generated 12-second synthetic fixture contains bass-like sine, gated harmonic tone, and decaying noise impulses. It tests execution and sample accounting only. It cannot establish vocal isolation, musical artifacts, bleed, or a winning preset.

Provide 20–60 second excerpts covering dub/reggae, dense rock, electronic 808/sub, vocal harmonies, acoustic/piano, and transient-heavy percussion. Include quiet passages, reverb tails, hard-panned content, and busy choruses. For objective quality scores supply aligned FLOAT WAV references for vocals/melody/bass/rhythm plus mixture. Reference MELODY must already combine the relevant instruments. Do not score estimated references from another model as ground truth.

Copy benchmark.example.json, replace paths relative to the manifest, and run:

```
python -m studio_stem_engine.benchmark corpus.json --output benchmark-results
```

The harness forces fresh inference output, records end-to-end seconds (including initialization/downloads), real-time factor, settings, hashes and reconstruction metadata. It writes report.json after each run and records failed runs. Run warm repeats after downloads for useful timing comparisons; first-run timings are not throughput benchmarks. Temporary stems are discarded by the harness; use the CLI for files you want to audition.

With references it reports zero-mean, globally scaled SI-SDR and scale-sensitive waveform SNR. These are whole-excerpt metrics, not museval's windowed/filter-based BSS Eval SDR. Silent references get no dB score. It rejects different reference sample grids. Without references quality scores are explicitly absent.

Ablations: Fast, Balanced, Best; primary-only versus adding second-stage vocals (current Best adds them); melody/energy/no consistency; BS versus Mel-Band vocal models; four-source versus six-source instrument models; controlled weighted ensembles. Phase and gain must match before any waveform averaging.

Listening rubric (blind matched-gain randomized A/B): vocal completeness/backing layers; vocal leakage into MELODY; missing chord notes; bass weight/808 sustain; drum attacks/cymbals; warbling and reverberant artifacts. Audition solos, reconstructed mix, and actual fader-mute/FX use. Report per-track failures as well as averages. A low reconstruction error is a correctness property, not a quality score.

## Extended models

See [extended benchmark and training interfaces](EXTENDED_STEMS.md). Use
`benchmark.extended.example.json` for baseline, six-source and configured RoFormer
comparisons. Fallback is disabled for benchmarks. Model weights and audio are external.
