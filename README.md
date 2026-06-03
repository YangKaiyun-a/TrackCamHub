# TrackCamHub

TrackCamHub is the Windows-side service that listens for the track/sample-ready signal and triggers the Linux camera board through Thrift. It also exposes the `SampleRegUC` Thrift service so the camera board can report task completion through `TaskInfoChanged`.

## Current Shape

- `src/serial`: Windows serial port reader and sample-ready listener.
- `src/thrift`: `SampleRegLC` client, `SampleRegUC` server, and vendored Thrift IDL/generated code.
- `src/workflow`: capture task state machine and `TaskInfoChanged` callback handling.
- `src/config`: simple `key=value` config loader.
- `config/trackcamhub.ini`: first-run configuration.

## Build

The Thrift headers and native runtime libraries are vendored under `third_party/`. Those libraries are MSVC binaries, so build this project with an MSVC toolchain.

```bat
call "D:\Microsoft Visual Studio\2026\Community\VC\Auxiliary\Build\vcvars64.bat"
cmake -S . -B build-msvc -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build-msvc
```

Run:

```bat
build-msvc\TrackCamHub.exe config\trackcamhub.ini
```

The default config disables serial listening and points the camera client at `127.0.0.1:9090`, so heartbeat failures are expected until a camera Thrift service is running.

## Vendored Runtime

- `third_party/thrift`: Apache Thrift headers copied from Federica.
- `third_party/native-runtime/lib/x64`: MSVC import/static libraries.
- `third_party/native-runtime/bin/x64`: runtime DLLs copied after build.

## Next Device Details Needed

- Exact serial frame format from the track/lower controller.
- Exact camera task type list and `mode` required for this capture workflow.
- Whether this Windows service should later run as a real Windows Service instead of a console process.
