# Extended stem validation

Final checks on this Mac:

- Stem-engine/service suite: **73 passed**, including 23 new cases, with two
  existing FastAPI/Starlette deprecation warnings. All original 50 cases pass.
- Repository `make test`: **5,153 checks in 11 groups** and **8 Python importer tests** pass.
- Companion JavaScript passes `node --check`.
- Python 0.2.0 wheel builds successfully using the installed cached setuptools backend.
  The inference environment has no pip/setuptools installed, so its cached build backend
  was supplied through PYTHONPATH; no ML dependency installation was changed.
- Real CPU Demucs inference on a generated 2-second stereo fixture: htdemucs produces
  four outputs; htdemucs_6s produces six (vocals, guitar, other, piano, bass, drums).
  Both have 88,200 frames at 44.1 kHz. Maximum reconstruction errors were
  3.73e-9 and 1.86e-9. Both PCM16 transfer exports pass verification.
- Isolated localhost companion, real upload -> decode -> dynamic six-source inference
  -> export -> authenticated downloads: ready; every downloaded SHA-256 matches.
  Dynamic mode folded low-energy estimates into four family outputs on this tone fixture.
  This is expected signal filtering, not a requirement to expose six channels on every input.
- YouTube acquisition wiring is exercised with an injected provider fixture; the existing
  media acquisition code is unchanged. No live YouTube download was performed this turn.
- RoFormer adapter source labels/gain behavior is tested with an explicitly simulated
  runtime. No full-source RoFormer checkpoint was available for real inference.
- Tests cover channel caps/banks, confidence validation and folding, atomic failure,
  fallback provenance, v1/v2 export rejection boundaries, checksum corruption, cache reuse,
  request replay conflicts, benchmark group comparison, and training split leakage/mixtures.

Run from the service directory after installing `.[test,cloud]`:

```
python -m pytest tests -q
```

Run `make test` at the repository root. Use the declared build-system backend to build
its wheel. Real model runs require the optional ML dependencies and cached/downloadable
weights. Smoke timings include startup and are not representative performance rankings.

No proprietary training, music-quality improvement, sixteen-track hardware playback,
full-source RoFormer inference, live YouTube download, or physical device upload is claimed.
The existing companion must be restarted to load its new routes and UI. The running
server and device firmware were not restarted/flashed by this change.
