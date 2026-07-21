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

## Capture Result Saving

TrackCamHub can save the selected result images and task result returned by the camera after a capture task finishes. The flow is:

1. The track serial signal triggers `SampleRegLC::DistributeTask`.
2. The camera finishes capture and calls `SampleRegUC::TaskInfoChanged`.
3. TrackCamHub saves `TaskInfo.result.bestBarcodeImage.bestBarcodeImage`,
   `TaskInfo.result.bestLiquidImage.bestLiquidImage`, `TaskInfo.imageOut`, and `result.json`.

Control `TaskInfo.imageOut` saving independently for each camera in `config/trackcamhub.ini`:

```ini
camera.camera-1.save_image_out_enabled=true
camera.camera-2.save_image_out_enabled=false
```

`bestBarcodeImage`, `bestLiquidImage`, and `result.json` are always saved when a
capture result is received. This switch only controls the `imageOut` directory.

Files are saved under:

```text
camera_images\<camera-id>\YYYYMMDD_HH_MM_SS
```

with names such as:

```text
best_barcode_image.ppm
best_liquid_image.ppm
imageOut\imageOut_1.ppm
imageOut\imageOut_2.ppm
result.json
```

Image formats use encodings that do not require extra libraries:

- grayscale image data is saved as `.pgm`
- BGR image data is converted to RGB and saved as `.ppm`
- unsupported byte counts are saved as `.bin`

The `result.json` file stores task metadata, saved image filenames, `resultFlags`, and `resultText`. The two selected image fields are saved as image files and are filtered out of `resultText` to avoid duplicating image data in JSON.

For multiple cameras, bind each camera id to its own track serial port:

```ini
camera.ids=camera-1,camera-2

camera.camera-1.host=172.30.1.111
camera.camera-1.port=7082
camera.camera-1.save_image_out_enabled=true
track.camera-1.serial_enabled=true
track.camera-1.port=COM7

camera.camera-2.host=172.30.1.112
camera.camera-2.port=7082
camera.camera-2.save_image_out_enabled=false
track.camera-2.serial_enabled=true
track.camera-2.port=COM8
```

## Direct Capture Trigger

TrackCamHub can trigger a capture task without a serial device. Enable the local TCP trigger in `config/trackcamhub.ini`:

```ini
track.camera-1.serial_enabled=false
direct_trigger.enabled=true
direct_trigger.host=127.0.0.1
direct_trigger.port=7090
```

Start TrackCamHub, then run:

```bat
trigger_capture.bat
```

or call the trigger client directly:

```bat
TriggerCapture.exe --host 127.0.0.1 --port 7090 --message CAPTURE
```

External programs can also open a TCP connection to `127.0.0.1:7090` and send one line of text. Each accepted request triggers the camera capture workflow directly and does not wait for the serial rotation-success frame.

## Serial Rotation Handshake

When the serial listener is enabled, TrackCamHub waits for the rotation-success frame before dispatching the camera task:

```ini
track.camera-1.serial_enabled=true
```

Flow:

1. Lower controller sends the 10-byte camera-position frame: `0x7e`, sequence, gripper id, command `0x00`, two reserved bytes, speed, checksum, `0xe7`.
2. TrackCamHub replies with the 7-byte `0x00` frame on the same serial port and stores the pending capture event.
3. Lower controller rotates the sample base, then sends the 7-byte `0x2c` frame when rotation succeeds.
4. TrackCamHub dispatches the camera capture task, saves the result, and waits for the matching 7-byte `0x3c` frame.
5. TrackCamHub sends a matching 7-byte `0x4c` frame to allow the lower controller to move the tube out.

If the lower controller sends `0x29`, TrackCamHub logs rotation failure and drops the pending capture event.

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
log\YYYY-MM-DD.log
```

## Vendored Runtime

- `third_party/thrift`: Apache Thrift headers copied from Federica.
- `third_party/native-runtime/lib/x64`: MSVC import/static libraries.
- `third_party/native-runtime/bin/x64`: runtime DLLs copied after build.

## Next Device Details Needed

- Exact serial frame format from the track/lower controller.
- Exact camera task type list and `mode` required for this capture workflow.
- Whether this Windows service should later run as a real Windows Service instead of a console process.
