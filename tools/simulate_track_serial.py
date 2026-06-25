import argparse
import itertools
import time

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


def main() -> int:
    parser = argparse.ArgumentParser(description="Simulate the track/lower-controller serial protocol.")
    parser.add_argument("--port", required=True, help="Serial port used by this simulator, for example COM8.")
    parser.add_argument("--baud", type=int, default=115200, help="Baud rate. Default: 115200.")
    parser.add_argument("--command", type=parse_int, default=0x00, help="Ready command byte. Default: 0x00.")
    parser.add_argument("--gripper", type=parse_int, default=1, help="Gripper id byte. Default: 1.")
    parser.add_argument("--sequence", type=parse_int, default=1, help="First sequence number. Default: 1.")
    parser.add_argument("--interval", type=float, default=0.0, help="Seconds between frames in loop mode.")
    parser.add_argument("--count", type=int, default=1, help="Number of frames to send. Use 0 for infinite.")
    parser.add_argument("--payload-hex", default="", help="Optional extra payload bytes, for example: '01 02 7E'.")
    args = parser.parse_args()

    payload = bytes.fromhex(args.payload_hex.replace("0x", "").replace(",", " "))
    total = itertools.count() if args.count == 0 else range(args.count)

    print(f"Opening {args.port} at {args.baud} baud...")
    with serial.Serial(args.port, args.baud, timeout=1) as port:
        for offset in total:
            sequence = (args.sequence + offset) & 0xFFFF
            frame = build_frame(sequence, args.gripper, args.command, payload)
            port.write(frame)
            port.flush()
            print(
                f"sent sequence={sequence} gripper={args.gripper} "
                f"command=0x{args.command & 0xFF:02X}: {to_hex(frame)}"
            )

            if args.count != 1:
                time.sleep(args.interval)

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
