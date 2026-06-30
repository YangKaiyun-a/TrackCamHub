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
call "D:\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat" amd64
cmake -S . -B build-msvc -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build-msvc
```

Run in console mode:

```bat
build-msvc\TrackCamHub.exe --console config\trackcamhub.ini
```

The default config disables serial listening and points the camera client at `127.0.0.1:9090`, so heartbeat failures are expected until a camera Thrift service is running.

Run as a Windows Service:

```bat
TrackCamHub.exe --service C:\TrackCamHub\config\trackcamhub.ini
```

The service mode must be launched by the Windows Service Control Manager. Use the packaged `install_service.bat` script instead of starting `--service` manually from a console.

## Camera Image Capture

TrackCamHub can periodically request the current camera image through `SampleRegLC::DistributeOper(GetCameraImage)`. The image data is received from the camera through the `SampleRegUC::OperInfoChanged` callback.

Enable it in `config/trackcamhub.ini`:

```ini
camera.image_capture_enabled=true
camera.image_capture_interval_ms=100
camera.image_capture_dir=camera_images
```

Saved files use formats that do not require extra libraries:

- grayscale image data is saved as `.pgm`
- BGR image data is converted to RGB and saved as `.ppm`
- unsupported byte counts are saved as `.bin`

## Package

From a Visual Studio Developer PowerShell or another shell where MSVC and Ninja are available:

```powershell
.\scripts\package.ps1
```

Or double-click the root packaging script:

```bat
package.bat
```

The package is written to:

```text
dist\TrackCamHub
```

The package includes `TrackCamHub.exe`, project runtime DLLs, configuration files, documentation, run scripts, and local MSVC/UCRT runtime DLLs. Copy that whole directory to the target Windows host. Then either run:

```bat
run_console.bat
```

or install it as a service from an Administrator command prompt:

```bat
install_service.bat
```

Service logs are written to:

```text
logs\TrackCamHub.log
```

## Vendored Runtime

- `third_party/thrift`: Apache Thrift headers copied from Federica.
- `third_party/native-runtime/lib/x64`: MSVC import/static libraries.
- `third_party/native-runtime/bin/x64`: runtime DLLs copied after build.

## Next Device Details Needed

- Exact serial frame format from the track/lower controller.
- Exact camera task type list and `mode` required for this capture workflow.
- Whether this Windows service should later run as a real Windows Service instead of a console process.
