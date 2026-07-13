# OBS AirPlay Receiver

[![Build](https://github.com/aomkoyo/obs-airplay-receiver/actions/workflows/build.yml/badge.svg)](https://github.com/aomkoyo/obs-airplay-receiver/actions/workflows/build.yml)
[![License: LGPL v2.1](https://img.shields.io/badge/License-LGPL_v2.1-blue.svg)](LICENSE)
[![Release](https://img.shields.io/github/v/release/aomkoyo/obs-airplay-receiver)](https://github.com/aomkoyo/obs-airplay-receiver/releases)

An OBS Studio plugin that receives **AirPlay screen mirroring** from Apple devices (iPhone, iPad, Mac) on Windows and displays it as a video source with synchronized audio.

> **Built entirely with [Claude Code](https://claude.ai/code)** (Anthropic's AI coding agent) — from protocol implementation to CI/CD pipeline. This project is a Windows port of [mika314/obs-airplay](https://github.com/mika314/obs-airplay), using [UxPlay](https://github.com/FDH2/UxPlay)'s battle-tested AirPlay 2 protocol library.

## Features

- **Screen Mirroring** - Receive AirPlay screen mirroring as a native OBS source
- **Audio Support** - AAC audio decoded and synced with video
- **Configurable Resolution** - Set width and height to match your scene layout
- **Configurable FPS** - Control the maximum frame rate of the mirrored stream
- **Auto Discovery** - mDNS/Bonjour advertisement so Apple devices find the receiver automatically
- **Hardware Decoding** - Tries NVIDIA NVDEC, Intel QSV, and D3D11VA before falling back to software H.264 decoding
- **Low Latency** - Direct H.264 frame pipeline with minimal buffering

## Requirements

- **OBS Studio 30+** (tested with 32.1.1)
- **Windows 10/11 64-bit**
- **Apple Bonjour** - Required for mDNS discovery so Apple devices can find the receiver. Install [Bonjour Print Services](https://support.apple.com/kb/DL999) package.

## Installation

1. Download the latest release `.zip` from the [Releases](../../releases) page
2. Extract the zip contents
3. Run `install.bat` as Administrator, or manually copy the files:
   - `obs-airplay-receiver.dll` to `%PROGRAMDATA%\obs-studio\plugins\obs-airplay-receiver\bin\64bit\`
   - `libcrypto-3-x64.dll` to the same directory
4. Restart OBS Studio

To uninstall, run `uninstall.bat` as Administrator or delete the `%PROGRAMDATA%\obs-studio\plugins\obs-airplay-receiver` folder.

## Usage

1. Start OBS Studio
2. Click **+** under Sources and select **AirPlay Receiver**
3. Configure the source settings:
   - **Server Name** - How the receiver appears on Apple devices (default: "OBS AirPlay Receiver")
   - **Width / Height** - Resolution of the received stream
   - **Max FPS** - Maximum frame rate
4. On your Apple device, open **Control Center** > **Screen Mirroring** and select the server name
5. The mirrored screen appears in OBS with audio

### Firewall

If your Apple device cannot find the receiver, make sure Windows Firewall allows:
- **TCP port 7000** (AirPlay)
- **UDP port 5353** (mDNS/Bonjour)

## Building from Source

### Prerequisites

- Visual Studio 2019 or 2022 with C/C++ workload (MSVC compiler)
- CMake 3.16+
- OpenSSL (install via `choco install openssl` or Scoop)
- Git (for submodules)

### Steps

All commands below should be run from a **VS Developer Command Prompt**.

1. **Clone with submodules**
   ```bash
   git clone --recursive https://github.com/aomkoyo/obs-airplay-receiver.git
   cd obs-airplay-receiver
   ```

2. **Download dependencies** into the `deps/` directory:
   - **OBS Studio SDK** (32.1.1) - extract to `deps/obs-sdk/obs-studio-32.1.1/`
     ```bash
     curl -L -o deps/obs-sdk.zip https://github.com/obsproject/obs-studio/archive/refs/tags/32.1.1.zip
     cd deps && mkdir obs-sdk && cd obs-sdk && tar -xf ../obs-sdk.zip && cd ..\..
     ```
   - **FFmpeg 7.1 headers** - extract to `deps/ffmpeg7-include/`
   - **libplist 2.7.0** - extract to `deps/libplist-2.7.0/`

3. **Generate import libraries** from OBS DLLs:
   ```bat
   lib /def:deps\obs.def /out:deps\obs.lib /machine:x64
   lib /def:deps\w32-pthreads.def /out:deps\w32-pthreads.lib /machine:x64
   lib /def:deps\dnssd.def /out:deps\dnssd.lib /machine:x64
   ```

4. **Build libplist**
   ```bat
   cd deps\libplist-2.7.0
   mkdir build && cd build
   cmake .. -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=cl
   nmake
   cd ..\..\..
   ```

5. **Build UxPlay libraries** (playfair, llhttp, airplay)
   ```bat
   cd deps\uxplay-build
   mkdir build && cd build
   cmake .. -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=cl
   nmake
   cd ..\..\..
   ```

6. **Build the plugin**
   ```bat
   mkdir build && cd build
   cmake .. -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=cl
   nmake
   ```

7. The output `obs-airplay-receiver.dll` will be in the `build/` directory.

## Troubleshooting

- **Device doesn't see the receiver** - Check that Bonjour is installed and Windows Firewall allows TCP 7000 and UDP 5353
- **No audio** - Ensure the source's audio monitoring is enabled in the OBS audio mixer
- **One client at a time** - AirPlay screen mirroring supports a single connected device

## Built with Claude Code

This entire project — from initial protocol implementation through debugging, Windows porting, CI/CD pipeline, and installer — was built using [Claude Code](https://claude.ai/code), Anthropic's AI coding agent. The development process included:

- Porting UxPlay's POSIX-only AirPlay library to Windows (patching pthreads, sockets, endianness, timing)
- Debugging real-time AirPlay connections with an actual iPhone
- Building a complete CI pipeline that downloads and compiles 5 dependencies from source
- Creating an Inno Setup installer for one-click installation

## Credits

- [UxPlay](https://github.com/FDH2/UxPlay) - Open-source AirPlay 2 server (core protocol: FairPlay, pairing, encryption)
- [mika314/obs-airplay](https://github.com/mika314/obs-airplay) - Original OBS AirPlay plugin for Linux (inspiration and reference)
- [FFmpeg](https://ffmpeg.org/) - H.264 and AAC-ELD decoding
- [OBS Studio](https://obsproject.com/) - Plugin API
- [libplist](https://github.com/libimobiledevice/libplist) - Apple binary plist format
- [OpenSSL](https://www.openssl.org/) - Cryptography (AES, SHA, Ed25519)

## Contributing

Contributions welcome! Please open an issue or pull request.

## License

LGPL-2.1 — same as UxPlay, which this project depends on. See [LICENSE](LICENSE).
