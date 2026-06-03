# TrackCamHub Architecture

TrackCamHub is the Windows-side coordinator between the track controller and the Linux camera board.

```text
Track / motor controller -- serial --> TrackCamHub on Windows -- Thrift/TCP --> Linux camera board
                                                                       ^
                                                                       |
                                                   TaskInfoChanged callback server
```

The first implementation keeps three responsibilities separate:

- `serial`: listens for the track sample-ready signal.
- `workflow`: accepts a sample-ready event, starts one camera capture task, and waits for `TaskInfoChanged`.
- `thrift`: calls `SampleRegLC::HeartbeatToLC` for heartbeat, calls `SampleRegLC::DistributeTask` for capture, and hosts `SampleRegUC` for `TaskInfoChanged`.

The Thrift IDL, generated C++ files, headers, and runtime libraries are vendored in this repository.
