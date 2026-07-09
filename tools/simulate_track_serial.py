import argparse
import itertools
import time
from typing import Optional

try:
    import serial
except ImportError as exc:
    raise SystemExit(
        "pyserial is required. Install it with: python -m pip install pyserial"
    ) from exc


FRAME_HEAD = 0x7E
FRAME_TAIL = 0xE7
ESCAPE = 0x1B
ESCAPE_HEAD = 0xEA
ESCAPE_TAIL = 0xEB
ESCAPE_ESCAPE = 0x00
READY_COMMAND = 0x00
ROTATION_SUCCESS_COMMAND = 0x2C
ROTATION_FAILURE_COMMAND = 0x29
CAMERA_ACK_COMMAND = 0x00


def parse_int(value: str) -> int:
    return int(value, 0)


def checksum(payload: bytes) -> int:
    return sum(payload) & 0xFF


def escape_payload(payload: bytes) -> bytes:
    escaped = bytearray()
    for byte in payload:
        if byte == FRAME_HEAD:
            escaped.extend((ESCAPE, ESCAPE_HEAD))
        elif byte == FRAME_TAIL:
            escaped.extend((ESCAPE, ESCAPE_TAIL))
        elif byte == ESCAPE:
            escaped.extend((ESCAPE, ESCAPE_ESCAPE))
        else:
            escaped.append(byte)
    return bytes(escaped)


def unescape_payload(payload: bytes) -> Optional[bytes]:
    output = bytearray()
    escaping = False
    for byte in payload:
        if escaping:
            if byte == ESCAPE_HEAD:
                output.append(FRAME_HEAD)
            elif byte == ESCAPE_TAIL:
                output.append(FRAME_TAIL)
            elif byte == ESCAPE_ESCAPE:
                output.append(ESCAPE)
            else:
                return None
            escaping = False
            continue

        if byte == ESCAPE:
            escaping = True
            continue

        output.append(byte)

    if escaping:
        return None
    return bytes(output)


def build_frame(sequence: int, gripper_id: int, command: int, payload: bytes = b"") -> bytes:
    body = bytearray()
    body.append((sequence >> 8) & 0xFF)
    body.append(sequence & 0xFF)
    body.append(gripper_id & 0xFF)
    body.append(command & 0xFF)
    body.extend(payload)
    body.append(checksum(body))
    return bytes([FRAME_HEAD]) + escape_payload(body) + bytes([FRAME_TAIL])


def to_hex(data: bytes) -> str:
    return " ".join(f"0x{byte:02X}" for byte in data)


def parse_frame(raw_frame: bytes):
    if len(raw_frame) < 2 or raw_frame[0] != FRAME_HEAD or raw_frame[-1] != FRAME_TAIL:
        return None

    body = unescape_payload(raw_frame[1:-1])
    if body is None or len(body) < 5:
        return None

    actual_checksum = body[-1]
    data = body[:-1]
    if checksum(data) != actual_checksum:
        return None

    sequence = (data[0] << 8) | data[1]
    gripper_id = data[2]
    command = data[3]
    payload = data[4:]
    return sequence, gripper_id, command, payload


def send_frame(port, sequence: int, gripper_id: int, command: int, payload: bytes = b"") -> None:
    frame = build_frame(sequence, gripper_id, command, payload)
    port.write(frame)
    port.flush()
    print(
        f"sent sequence={sequence} gripper={gripper_id} "
        f"command=0x{command & 0xFF:02X}: {to_hex(frame)}"
    )


def read_frame(port, timeout: float) -> Optional[bytes]:
    deadline = time.monotonic() + timeout
    raw = bytearray()
    in_frame = False

    while time.monotonic() < deadline:
        chunk = port.read(1)
        if not chunk:
            continue

        byte = chunk[0]
        if not in_frame:
            if byte == FRAME_HEAD:
                raw.clear()
                raw.append(byte)
                in_frame = True
            continue

        raw.append(byte)
        if byte == FRAME_TAIL:
            return bytes(raw)

        if len(raw) > 512:
            raw.clear()
            in_frame = False

    return None


def wait_for_ack(port, sequence: int, gripper_id: int, ack_command: int, timeout: float) -> bool:
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        remaining = max(0.0, deadline - time.monotonic())
        raw_frame = read_frame(port, remaining)
        if raw_frame is None:
            break

        parsed = parse_frame(raw_frame)
        print(f"received: {to_hex(raw_frame)}")
        if parsed is None:
            print("ignored invalid response frame")
            continue

        response_sequence, response_gripper, response_command, _ = parsed
        if (
            response_sequence == sequence
            and response_gripper == gripper_id
            and response_command == (ack_command & 0xFF)
        ):
            print(
                f"ack matched sequence={response_sequence} gripper={response_gripper} "
                f"command=0x{response_command:02X}"
            )
            return True

        print(
            f"ignored unmatched response sequence={response_sequence} "
            f"gripper={response_gripper} command=0x{response_command:02X}"
        )

    print(
        f"ack timeout sequence={sequence} gripper={gripper_id} "
        f"expected_command=0x{ack_command & 0xFF:02X}"
    )
    return False


def main() -> int:
    parser = argparse.ArgumentParser(description="Simulate the track/lower-controller serial protocol.")
    parser.add_argument("--port", required=True, help="Serial port used by this simulator, for example COM8.")
    parser.add_argument("--baud", type=int, default=115200, help="Baud rate. Default: 115200.")
    parser.add_argument(
        "--mode",
        choices=("sequence", "single", "fail"),
        default="sequence",
        help="sequence sends ready, waits for software ack, then sends rotation-success. single sends only --command. fail sends ready, waits for ack, then sends rotation-failure.",
    )
    parser.add_argument("--command", type=parse_int, default=READY_COMMAND, help="Command byte for single mode.")
    parser.add_argument("--gripper", type=parse_int, default=1, help="Gripper id byte. Default: 1.")
    parser.add_argument("--sequence", type=parse_int, default=1, help="First sequence number. Default: 1.")
    parser.add_argument("--interval", type=float, default=0.0, help="Seconds between frames in loop mode.")
    parser.add_argument("--count", type=int, default=1, help="Number of frames to send. Use 0 for infinite.")
    parser.add_argument(
        "--rotation-delay",
        type=float,
        default=0.2,
        help="Seconds between matched ack and rotation result in sequence/fail mode. Default: 0.2.",
    )
    parser.add_argument(
        "--ack-timeout",
        type=float,
        default=3.0,
        help="Seconds to wait for software ack before giving up. Default: 3.0.",
    )
    parser.add_argument(
        "--ack-command",
        type=parse_int,
        default=CAMERA_ACK_COMMAND,
        help="Software ack command byte expected before sending rotation result. Default: 0x00.",
    )
    parser.add_argument(
        "--ready-command",
        type=parse_int,
        default=READY_COMMAND,
        help="Ready command byte used in sequence/fail mode. Default: 0x00.",
    )
    parser.add_argument(
        "--success-command",
        type=parse_int,
        default=ROTATION_SUCCESS_COMMAND,
        help="Rotation success command byte used in sequence mode. Default: 0x2c.",
    )
    parser.add_argument(
        "--failure-command",
        type=parse_int,
        default=ROTATION_FAILURE_COMMAND,
        help="Rotation failure command byte used in fail mode. Default: 0x29.",
    )
    parser.add_argument("--payload-hex", default="", help="Optional extra payload bytes, for example: '01 02 7E'.")
    args = parser.parse_args()

    payload = bytes.fromhex(args.payload_hex.replace("0x", "").replace(",", " "))
    total = itertools.count() if args.count == 0 else range(args.count)

    print(f"Opening {args.port} at {args.baud} baud...")
    with serial.Serial(args.port, args.baud, timeout=1) as port:
        for offset in total:
            sequence = (args.sequence + offset) & 0xFFFF
            if args.mode == "single":
                send_frame(port, sequence, args.gripper, args.command, payload)
            else:
                send_frame(port, sequence, args.gripper, args.ready_command, payload)
                if not wait_for_ack(port, sequence, args.gripper, args.ack_command, args.ack_timeout):
                    if args.count != 1:
                        time.sleep(args.interval)
                    continue

                if args.rotation_delay > 0:
                    time.sleep(args.rotation_delay)
                rotation_command = args.success_command if args.mode == "sequence" else args.failure_command
                send_frame(port, sequence, args.gripper, rotation_command)

            if args.count != 1:
                time.sleep(args.interval)

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
