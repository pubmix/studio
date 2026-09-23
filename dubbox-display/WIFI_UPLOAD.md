# Wi-Fi upload, stem preparation and lossless transfer

## Implemented

The existing ESP32-served page at `/` remains the upload UI. Off mode uploads ordinary
audio without a companion. Compatible 44.1 kHz PCM16 mono/stereo RIFF WAV files are
preserved byte-for-byte; other browser-decodable audio uses the previous conversion
to 44.1 kHz PCM16 stereo. Select options before choosing/dropping files. Each queued
item snapshots its settings and companion connection. Original/uncompressed WAV is
the default. No lossy audio codec was added.

Four, extended and dynamic splitting use the existing authenticated stem-engine
`/v1/models`, `/v1/uploads`, job status/cancel and WAV download routes. The browser
uploads the source to the companion, waits for verified publication, then sends the
four prepared files sequentially to this page's device. Source separation stays off
both microcontrollers. Automatic model selection uses the service's four/six-source
Demucs profiles; configured deeper models remain available. Model availability,
weights and resulting source count are not guaranteed by the UI. Fallback is disclosed.

Deeper sources appear as authenticated download buttons. Hardware still has four
playback tracks: only VOCALS, MELODY, BASS, RHYTHM mixdown is delivered automatically.
No 16-track playback or automatic track assignment is claimed. Use the device picker.
The companion's existing YouTube page and ingestion backend remain available.

## Pairing

Run the existing companion with `--device http://dubbox.local` when viewing exactly
that origin, or substitute the device IP origin if using its IP. CORS permits only
that configured origin (no wildcard); all API operations still require the bearer key.
On the companion page, expand “Pair the Dub-Box Wi-Fi upload page” and reveal/copy the
existing key. Enter it and the companion origin in the device upload page, then
Connect engine. Ordinary browser splitting keeps the key in memory. The optional explicit “Enable
touchscreen controls” action sends the companion origin/key to this Dub-Box for NVS
storage. Keys are never placed in query strings or SD files. See STEM_TOUCHSCREEN.md.

For another computer/phone, use the companion computer's LAN address and the server's
existing explicit `--host 0.0.0.0` option on a trusted private network. Loopback refers
to the browsing device. Browser local-network permission may be needed. Authentication
and CORS do not encrypt HTTP; existing trusted-LAN scope still applies. A running older
companion must be restarted to load the new CORS and pairing UI. Job limits remain
100 MiB / 15 minutes, with server timeout/cancellation unchanged. Direct uploads keep
the receiver's existing size limit; browser memory can impose a lower practical limit.

## Transfer format: deflate-blocks-v1

The Teensy SD playback path requires PCM16 WAV. Therefore compression is transport-only
and decoded on ESP32 before the existing CRC-protected UART WAV upload. No Teensy
firmware/protocol changes are required. MP3/AAC/Opus/FLAC playback support is not assumed.

`GET /status` adds `transfer_modes` (`wav`, and `deflate-blocks-v1` when the bounded
buffers are allocated) and `playback_channels: 4`. Existing status fields remain.
`POST /upload?name=NAME&size=DECODED_BYTES` with one multipart file remains valid.
Compressed requests add `encoding=deflate-blocks-v1&wire_size=ENCODED_BYTES`.

Body file bytes are concatenated blocks:

- uint16 little-endian encoded byte length (1..8256)
- uint16 little-endian decoded byte length (1..8192)
- one complete zlib-wrapped DEFLATE stream including its Adler32 checksum

The browser uses native `CompressionStream('deflate')` for each 8192-byte slice.
The ESP32 uses its SDK/ROM tinfl implementation; each stream must finish, consume
exactly the advertised bytes and produce exactly its bounded output size. No global
archive/file extraction, filenames or filesystem operations exist in this decoder.
`transfer_blocks.h` buffers at most one encoded/decoded block; HTTP fragmentation is
independent of block boundaries. Decoded bytes use the existing backpressured ring.

Validation rejects unknown encodings, invalid/oversized lengths, checksum failures,
truncation, trailing data and multiple multipart files. The final UART chunk is held
until the request is validated, preventing the Teensy from committing before that
check. Failure cancels the existing upload; the Teensy removes its incomplete file.
Confirmed earlier stems remain on SD. No automatic retry, rollback or remote checksum
query was added. Inspect SD after connection failures before retrying.

## Size and speed

The page displays prepared WAV size, actual transmitted size, reduction percentage,
selected/effective transfer mode, upload progress, SD acknowledgement, elapsed delivery
time, saved filenames and confirmed partial successes. An unhelpful compression result
falls back explicitly to uncompressed WAV. Missing browser/device compression support
produces an actionable error; it never silently sends compressed data to old firmware.

Compression reduces only browser-to-ESP32 traffic. SD size and ESP32-to-Teensy bytes
are identical. The documented roughly 190 kB/s serial link remains a likely bottleneck;
compression can add preparation time without improving total delivery time. A single short physical test is recorded below; it is not a general speedup guarantee. Splitting also sends a source to the companion and
retrieves stem WAVs before device delivery; that traffic is separate.

On four previously prepared local song stems, each 43,667,504 bytes:

| Stem | Compressed bytes | Reduction | Compression time on this Mac |
| --- | ---: | ---: | ---: |
| Vocals | 34,775,905 | 20.36% | 1.56 s |
| Melody | 38,081,639 | 12.79% | 1.37 s |
| Bass | 33,368,954 | 23.58% | 1.41 s |
| Rhythm | 33,320,229 | 23.70% | 1.39 s |

Measured with the actual page compressor in Node 24; every block decoded to the exact
original bytes. These are fixture-specific results, not codec quality or network speed
benchmarks. Music/noise can shrink less. Compression streams reference:
https://developer.mozilla.org/en-US/docs/Web/API/CompressionStream/CompressionStream

## Validation and limits

See [host test instructions](tests/README.md). ESP32 PlatformIO build passed. All 75
stem service tests passed (two existing deprecation warnings), including scoped CORS
and auth; existing firmware tests passed 5,153 checks and eight importer tests. Browser
page syntax and simulated upload/split/partial failure paths passed. Desktop layout and
expanded pairing controls were inspected in a real browser using a local static preview.
The preview's disconnected status is expected; it is not a device response.

The block parser passed AddressSanitizer/UndefinedBehaviorSanitizer tests for fragmented
headers/payloads, every possible truncation of a fixture, checksum corruption, excess
output, trailing bytes, invalid sizes and a failed output sink. Native browser-produced
blocks also passed through the actual C++ parser and matched the WAV. Host inflation
uses zlib as a substitute for the ESP32 ROM; the real ROM path was also exercised on the installed ESP32 with successful SD acknowledgements. Physical playback and remote byte readback remain unverified.

No live YouTube download or trained/new model quality claim is included in this change. Existing YouTube provider fixtures and engine tests passed.

## Physical deployment and integration checks

The verified ESP32 was flashed on 2026-09-23 with all programmed-region hashes
verified. Its recorder was stopped/restarted around flashing. Startup reported
touch initialization OK, link UP, and a valid/live zero-drop drawing-command mirror.
Final build used 123,724 bytes static RAM and 969,377 bytes flash; compression buffers
also allocate bounded heap memory. Teensy firmware was not changed.

The actual device-served page was inspected in a browser and showed Connected/ready
with the new controls. The existing idle companion was restarted preserving jobs and
the pairing key; authenticated requests from the configured origin succeeded.

A generated two-second stereo tone was saved through both physical transfer paths:
352,844 original bytes in 8.279 seconds versus 189,413 compressed bytes in 6.969 seconds
(46.3% fewer Wi-Fi payload bytes, 15.8% less elapsed delivery time in this single trial).
This excludes preparation and is not a representative music/network benchmark. A
corrupted final compressed block was rejected; serial telemetry confirmed cancellation.

A real companion job then ran Demucs six-source extended inference on a synthetic
two-second stereo fixture. All six extended downloads matched their manifest hashes;
the four canonical mixdown WAVs were compressed, delivered and acknowledged by the
Teensy. No source fallback was used. No playback was started and no track assignment
or project settings were changed. Six inspection files remain on SD: TEST-WIFI-RAW.wav,
TEST-WIFI-ZLIB.wav and TEST-SPLIT-{VOCALS,MELODY,BASS,RHYTHM}.wav. There is no remote SD
readback/checksum API; byte-exact codec checks were performed on host, while physical
tests establish decoder acceptance and SD completion rather than independent readback.
