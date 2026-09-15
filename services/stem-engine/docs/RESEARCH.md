# Separation research and selection

Primary sources checked 2026-09-14 (local date). Runtime evidence is separate in VERIFICATION.md. Published scores from different datasets/configurations are not treated as comparable STUDIO results.

| Candidate | Role in STUDIO | Integration |
|---|---|---|
| Spleeter 4-stem | Simple baseline | Isolated subprocess adapter; dependency installation failed here |
| Demucs htdemucs | Fast full-source candidate | Direct PyTorch and MLX adapters |
| Demucs htdemucs_ft | Balanced candidate | Fine-tuned bag through direct PyTorch |
| Demucs htdemucs_6s | Instrument stage, guitar/piano recombination | Direct PyTorch |
| BS-RoFormer | Strong dedicated vocal candidate | audio-separator and MLX |
| Mel-Band RoFormer | Alternative vocal candidate | MLX; selected checkpoint inference passed |
| MDX / MDXC | Alternative separation families | Selected MDX (ONNX CPU) and MDX23C (MLX) checkpoints passed; other checkpoints remain untested |
| Weighted/staged ensembles | Experimental error combination | Native orchestration and explicit waveform ensemble API |
| Future STUDIO-native model | Direct semantic four-stem output | Research direction only; no training completed |

## Demucs

The archived Meta repository points to the maintainer's continuation. The installed Demucs 4.1.0 identifies [adefossez/demucs](https://github.com/adefossez/demucs) as its homepage; that primary repository documents modernized packaging and Hugging Face weights. Its six-source model includes guitar and piano, with acknowledged piano bleed/artifacts. Recombining these sources into MELODY avoids exposing an unreliable piano lane, but does not guarantee an improved melodic estimate. The fine-tuned model bag costs more compute than the single-model version. [Original architecture and model descriptions](https://github.com/facebookresearch/demucs).

## RoFormer families

[BS-RoFormer paper](https://arxiv.org/abs/2309.02612) describes band-split spectrogram modeling with rotary-position Transformers. [Mel-Band RoFormer paper](https://arxiv.org/abs/2310.01809) explores overlapping mel-frequency bands. These architectures motivate dedicated vocal extraction; a specific community checkpoint's training and performance are separate questions. [Implementation](https://github.com/lucidrains/BS-RoFormer) and [Kimberley Jensen's vocal checkpoint project](https://github.com/KimberleyJensen/Mel-Band-Roformer-Vocal-Model) provide inspectable starting points.

The initial Best candidate uses `model_bs_roformer_ep_317_sdr_12.9755.ckpt`. The numeric filename is an upstream label, not a measured result here. The Mel-Band alternative is `vocals_mel_band_roformer.ckpt`. Karaoke/lead-only models are not interchangeable with the product requirement to preserve all vocal layers; do not silently substitute them.

## Runtimes and MDX

[audio-separator](https://github.com/nomadkaraoke/python-audio-separator) supports multiple model architectures and local CPU/MPS/CoreML paths. Its package metadata's older karaokenerds URL redirects to that project. The STUDIO adapter pins version 0.45.0 and bypasses per-source peak attenuation through instance configuration while preserving float output; that coupling must be retested on upgrades.

[mlx-audio-separator](https://github.com/ssmall256/mlx-audio-separator) ports these families to Apple Silicon MLX. Upstream publishes parity/speed validation, which is not automatically evidence for this host or STUDIO pipeline. The installed version is 0.1.5; a repaired native dependency build and approved GPU access were needed. STUDIO uses the same runtime boundary for BS/Mel RoFormer, MDX/MDXC and Demucs.

[ZFTurbo's training repository](https://github.com/ZFTurbo/Music-Source-Separation-Training) documents MDX23C and RoFormer configurations. [MVSEP-MDX23](https://github.com/ZFTurbo/MVSEP-MDX23-music-separation-model) provides a concrete multistage/ensemble reference. STUDIO does not claim to reproduce its competition recipe. MDX23C-family checkpoint testing remains distinct from testing RoFormer through a runtime class named MDXC.

## Spleeter

[Deezer's project](https://github.com/deezer/spleeter) documents 2/4/5-source modes and Apple M1 dependency issues. Installation failures observed here agree with the need for a separately managed baseline environment. No Spleeter inference or comparison score is claimed.

## Decision

Keep the backend boundary stable. Use the tested direct four-source path as a practical fallback. Evaluate the staged Best candidate against the single-pass fine-tuned bag on real multitracks before declaring a winner. Exact reconstruction can retain missing information while worsening leakage in MELODY; it is not a substitute for listening or reference metrics.

All candidates operate locally after obtaining dependencies/weights. Code and model licenses/training provenance differ: consult each primary source and checkpoint before redistribution. No commercialization clearance is asserted, and licensing did not determine this prototype's quality-oriented selection.
