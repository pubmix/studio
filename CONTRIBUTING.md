# Contributing to STUDIO

See [component placement and integration](docs/CONTRIBUTING_COMPONENTS.md) before importing another implementation.

## Work on a component

1. Create a feature branch from `main`.
2. Keep changes within the component's folder, plus shared contracts or docs when necessary.
3. Run `make test`. For C++ changes also run `make sanitize` and the relevant firmware build.
4. Open a pull request describing the behavior, tests and remaining hardware limitations.

Automated checks run the desktop build and test suite on Linux and macOS, plus sanitizers and generated-WAV validation on Linux. These checks do not validate physical hardware or run the ML separator.

Keep the four-stem order fixed: VOCALS, MELODY, BASS, RHYTHM. Document breaking contract changes explicitly. Never label untested hardware behavior or placeholder DSP as production-ready.

The source is public; a project-wide license has not yet been selected. Do not infer an open-source license for third-party code or model weights.
