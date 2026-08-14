# CppOverdub
A header-only,single file audio library for C++, making it easier to process (Like pydub)

Source of inspiration : [pydub](https://github.com/jiaaro/pydub)

Requirements: [ffmpeg](https://ffmpeg.org/) (vendored: 9.0.1)

[openal-soft](https://github.com/kcat/openal-soft) (vendored: 1.25.2)

Current Version: 20260110

Feature proposals: see [FEATURES.md](FEATURES.md)

## Highlights

- I/O: `doOpen` / `doExport` (formats, codecs, bitrate) / `doGetMetadata` (tags)
- Editing: concat, overlay, slice, repeat, reverse, fade, gain, normalize, compressor, filters, pan
- DSP: `doSpeedUp`, `doShiftPitch` (phase vocoder), `getSpectrum` (FFT), EBU R128 loudness (`getLoudness*`, LUFS)
- Playback (OpenAL): 3D sources + listener, queue streaming, EFX reverb/echo, HRTF, seamless loop points
