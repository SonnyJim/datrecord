# IRIX DAT Audio Recorder
---
IRIX CLI tool to record WAV audio files directly to DAT (Digital Audio Tape) using the [SGI libdataudio library](https://techpubs.jurassic.nl/library/manuals/1000/007-1799-040/sgi_html/ch09.html)

## Hardware Requirements

This tool requires a DDS drive that explicitly supports hardware **Audio Mode (`MTAUD`)** aka [DAT firmware](https://wiki.philpem.me.uk/computer/tapedrives/dds)

## Example usage

> datrecord file.wav /dev/tape

> datrecord playlist.txt /dev/tape 

## Supported WAV formats

* **Input File Format:** 16-bit PCM WAV (Uncompressed)
* **Supported Sample Rates:** 32 kHz, 44.1 kHz, 48 kHz
* **Channels:** Stereo (2 channels)

LP mode/4 channel mode not currently supported.  No sample rate conversion is performed
