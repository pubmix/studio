# STUDIO cross-component tests

Purpose: guard the native engine → P1 adapter → firmware media boundary and H0.1 pin/format expectations. Tests use the real engine publisher with a synthetic backend and run the C++ simulator; they do not download models or certify hardware. Run make integration-test from the repository root using the existing Stem Engine Python environment (NumPy, SciPy, SoundFile and imageio-ffmpeg). The required simulator builds automatically. See ../../docs/SYSTEM_ARCHITECTURE.md for status and remaining catalog, hardware and benchmark gates.
