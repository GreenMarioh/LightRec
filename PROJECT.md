# LightRec

An ultra-lightweight Windows clip recorder similar to ShadowPlay.

Goals:

- RAM < 50 MB idle
- CPU < 2% idle
- CPU < 5% while clipping
- Intel Iris Xe support
- Clip-first design
- Native C++20 application
- No Electron
- No telemetry

Primary features:

- Instant replay
- Ring buffer recording
- Hardware encoding:
  - Intel QSV
  - NVENC
  - AMD AMF
  - x264 fallback

Capture APIs:

- Windows Graphics Capture
- DXGI Desktop Duplication fallback

Output:

- MP4
- H.264
