"""Read ZH03B UART measurements in streaming or question-and-answer mode."""

import argparse
from datetime import datetime
import sys
import time


HEADER = b"\x42\x4d"
FRAME_SIZE = 24
SET_ACTIVE_COMMAND = bytes.fromhex("FF 01 78 40 00 00 00 00 47")
SET_QUERY_COMMAND = bytes.fromhex("FF 01 78 41 00 00 00 00 46")
QUERY_COMMAND = bytes.fromhex("FF 01 86 00 00 00 00 00 79")
QUERY_HEADER = b"\xff\x86"
QUERY_FRAME_SIZE = 9


def parse_frame(frame: bytes) -> tuple[int, int, int] | None:
    """Return PM1.0, PM2.5, PM10 (ug/m3), or None for an invalid frame."""
    if (
        len(frame) != FRAME_SIZE
        or frame[:2] != HEADER
        or frame[2:4] != b"\x00\x14"
        or sum(frame[:-2]) & 0xFFFF != int.from_bytes(frame[-2:], "big")
    ):
        return None
    return tuple(int.from_bytes(frame[i : i + 2], "big") for i in (10, 12, 14))


def parse_query_frame(frame: bytes) -> tuple[int, int, int] | None:
    """Return PM1.0, PM2.5, PM10 from a valid Q&A response."""
    if len(frame) != QUERY_FRAME_SIZE or frame[:2] != QUERY_HEADER or sum(frame[1:]) & 0xFF:
        return None
    return tuple(int.from_bytes(frame[i : i + 2], "big") for i in (6, 2, 4))


def take_frames(buffer: bytearray, header=HEADER, frame_size=FRAME_SIZE, parser=parse_frame):
    """Extract valid frames, retaining partial data for the next serial read."""
    while True:
        start = buffer.find(header)
        if start < 0:
            # Keep a possible first header byte split across serial reads.
            buffer[:] = buffer[-1:] if buffer.endswith(header[:1]) else b""
            return
        if start:
            del buffer[:start]
        if len(buffer) < frame_size:
            return
        frame = bytes(buffer[:frame_size])
        values = parser(frame)
        if values is None:
            del buffer[0]  # Search again without losing an embedded header.
            continue
        del buffer[:frame_size]
        yield values, frame


def main() -> int:
    parser = argparse.ArgumentParser(description="Test ZH03B UART output")
    parser.add_argument("--port", default="COM6", help="Serial port (default: COM6)")
    parser.add_argument("--count", type=int, default=0, help="Stop after N valid frames (default: run until Ctrl+C)")
    parser.add_argument("--raw", action="store_true", help="Also print each valid frame in hex")
    parser.add_argument("--dump", action="store_true", help="Print all received bytes in hex for UART troubleshooting")
    parser.add_argument("--set-active", action="store_true", help="Switch sensor to automatic upload mode (requires adapter TX to sensor RXD)")
    parser.add_argument("--query", action="store_true", help="Switch to Q&A mode and request measurements every second")
    args = parser.parse_args()
    if args.count < 0:
        parser.error("--count must be zero or greater")
    if args.query and args.set_active:
        parser.error("--query and --set-active cannot be used together")

    try:
        import serial
    except ImportError:
        print("Missing pyserial. Install it with: python -m pip install pyserial", file=sys.stderr)
        return 2

    try:
        with serial.Serial(args.port, baudrate=9600, bytesize=8, parity="N", stopbits=1, timeout=1) as port:
            if args.query:
                port.reset_input_buffer()
                port.write(SET_QUERY_COMMAND)
                port.flush()
                print("Sent Q&A mode command: " + SET_QUERY_COMMAND.hex(" "), flush=True)
                time.sleep(0.1)
            elif args.set_active:
                port.write(SET_ACTIVE_COMMAND)
                port.flush()
                print("Sent automatic upload command: " + SET_ACTIVE_COMMAND.hex(" "), flush=True)
            mode = "Q&A" if args.query else "automatic upload"
            print(f"Reading ZH03B on {args.port} (9600 8N1, {mode}). Press Ctrl+C to stop.", flush=True)
            buffer = bytearray()
            last_frame = time.monotonic()
            next_query = time.monotonic()
            received = 0
            while True:
                if args.query and time.monotonic() >= next_query:
                    port.write(QUERY_COMMAND)
                    port.flush()
                    if args.dump:
                        print("TX: " + QUERY_COMMAND.hex(" "), flush=True)
                    next_query = time.monotonic() + 1
                chunk = port.read(64)
                if args.dump and chunk:
                    print("RX: " + chunk.hex(" "), flush=True)
                buffer.extend(chunk)
                frames = (
                    take_frames(buffer, QUERY_HEADER, QUERY_FRAME_SIZE, parse_query_frame)
                    if args.query else take_frames(buffer)
                )
                for (pm1, pm25, pm10), frame in frames:
                    last_frame = time.monotonic()
                    received += 1
                    stamp = datetime.now().strftime("%H:%M:%S")
                    print(f"{stamp}  PM1.0={pm1:4d}  PM2.5={pm25:4d}  PM10={pm10:4d} ug/m3", flush=True)
                    if args.raw:
                        print("  " + frame.hex(" "), flush=True)
                    if args.count and received >= args.count:
                        return 0
                if time.monotonic() - last_frame >= 5:
                    print("No valid frame for 5 s. Check power, TX/GND wiring, COM port, and sensor mode.", file=sys.stderr, flush=True)
                    last_frame = time.monotonic()
    except KeyboardInterrupt:
        print("\nStopped.")
        return 0
    except serial.SerialException as exc:
        print(f"Could not use {args.port}: {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
