"""
UART Decoder for ESP Channel State Table messages.

Packet format (marker byte = 42):
  [0]      = 0x2A (42) — packet type identifier
  [1]      = total length (bytes) of this packet
  [2..N]   = payload:
               Look_up_entry  (packed): uint8 ID, float lat, float lon, int8 lifetime  → 10 bytes
               float P_Signal                                                           →  4 bytes
               float P_Channel                                                          →  4 bytes
               AntennaDir switch_state  (enum, stored as int/uint depending on compiler)→  4 bytes
               float Dist                                                               →  4 bytes
               float Heading                                                            →  4 bytes

AntennaDir enum:
    0=ERROR, 1=DIR_TX_OMNI, 2=DIR_RX_OMNI, 3=DIR_RX_0 … 10=DIR_RX_7
"""

import struct
import serial
import csv
import argparse
import time
import sys
from datetime import datetime
from pathlib import Path

# ── Constants ────────────────────────────────────────────────────────────────

PACKET_MARKER = 42

# Look_up_entry: __attribute__((packed))  →  uint8, float, float, int8
# Sizes: 1 + 4 + 4 + 1 = 10 bytes
LOOKUP_FMT  = "<BffbX"   # B=uint8(ID), f=lat, f=lon, b=int8(lifetime), X=pad to even (unused)
# Actually no padding needed for packed struct — use exact layout:
LOOKUP_FMT  = "<Bffb"    # 10 bytes total (1+4+4+1)
LOOKUP_SIZE = struct.calcsize(LOOKUP_FMT)   # = 10

FLOAT_FMT   = "<f"
FLOAT_SIZE  = struct.calcsize(FLOAT_FMT)   # = 4

# AntennaDir is a C++ enum — on most ESP32 toolchains this is int (4 bytes)
ENUM_FMT    = "<I"       # unsigned int, 4 bytes
ENUM_SIZE   = struct.calcsize(ENUM_FMT)

ANTENNA_DIR = {
    0: "ERROR",
    1: "DIR_TX_OMNI",
    2: "DIR_RX_OMNI",
    3: "DIR_RX_0",
    4: "DIR_RX_1",
    5: "DIR_RX_2",
    6: "DIR_RX_3",
    7: "DIR_RX_4",
    8: "DIR_RX_5",
    9: "DIR_RX_6",
    10: "DIR_RX_7",
}

# Expected minimum payload size
MIN_PAYLOAD = LOOKUP_SIZE + FLOAT_SIZE * 3 + ENUM_SIZE + FLOAT_SIZE  # P_Signal, P_Channel, Dist + switch + Heading
# = 10 + 4 + 4 + 4 + 4 + 4 = 30 bytes

CSV_HEADER = [
    "timestamp",
    "node_id",
    "latitude",
    "longitude",
    "lifetime",
    "P_Signal",
    "P_Channel",
    "switch_state",
    "Dist",
    "Heading",
]

# ── Parser ────────────────────────────────────────────────────────────────────

def parse_packet(data: bytes) -> dict | None:
    """
    Parse a single packet payload (everything after the 2-byte header).
    Returns a dict of decoded fields, or None on parse error.
    """
    offset = 0

    if len(data) < LOOKUP_SIZE:
        print(f"[WARN] Payload too short for Look_up_entry: {len(data)} bytes", file=sys.stderr)
        return None

    try:
        node_id, lat, lon, lifetime = struct.unpack_from(LOOKUP_FMT, data, offset)
        offset += LOOKUP_SIZE

        p_signal, = struct.unpack_from(FLOAT_FMT, data, offset); offset += FLOAT_SIZE
        p_channel, = struct.unpack_from(FLOAT_FMT, data, offset); offset += FLOAT_SIZE

        switch_raw, = struct.unpack_from(ENUM_FMT,  data, offset); offset += ENUM_SIZE
        switch_name = ANTENNA_DIR.get(switch_raw, f"UNKNOWN({switch_raw})")

        dist, = struct.unpack_from(FLOAT_FMT, data, offset); offset += FLOAT_SIZE

        heading = None
        if offset + FLOAT_SIZE <= len(data):
            heading, = struct.unpack_from(FLOAT_FMT, data, offset); offset += FLOAT_SIZE

    except struct.error as e:
        print(f"[WARN] Struct unpack error: {e}", file=sys.stderr)
        return None

    return {
        "node_id":      node_id,
        "latitude":     lat,
        "longitude":    lon,
        "lifetime":     lifetime,
        "P_Signal":     p_signal,
        "P_Channel":    p_channel,
        "switch_state": switch_name,
        "Dist":         dist,
        "Heading":      heading,
    }


# ── Stream reader ─────────────────────────────────────────────────────────────

def read_packets(port: serial.Serial):
    """
    Generator: yields raw payload bytes for each valid packet found in
    the byte stream. Scans for marker byte (42), reads length, reads payload.
    """
    buf = bytearray()

    while True:
        chunk = port.read(64)  # non-blocking read up to 64 bytes
        if chunk:
            buf.extend(chunk)

        # Scan for marker byte
        while len(buf) >= 2:
            marker_idx = buf.find(PACKET_MARKER)
            if marker_idx == -1:
                buf.clear()
                break
            if marker_idx > 0:
                print(f"[INFO] Discarding {marker_idx} bytes before marker", file=sys.stderr)
                del buf[:marker_idx]

            # We now have buf[0] == 42
            if len(buf) < 2:
                break  # wait for length byte

            length = buf[1]

            if length < 2:
                # Invalid length, skip this marker
                del buf[0]
                continue

            if len(buf) < length:
                break  # wait for more data

            packet = bytes(buf[2:length])  # payload (skip marker + length byte)
            del buf[:length]
            yield packet


# ── File reader (for --file mode) ────────────────────────────────────────────

def read_packets_from_bytes(raw: bytes):
    """Like read_packets but operates on a static bytes object."""
    i = 0
    while i < len(raw):
        idx = raw.find(PACKET_MARKER, i)
        if idx == -1:
            break
        if idx + 2 > len(raw):
            break
        length = raw[idx + 1]
        if length < 2 or idx + length > len(raw):
            i = idx + 1
            continue
        payload = raw[idx + 2: idx + length]
        yield payload
        i = idx + length


# ── CSV writer ────────────────────────────────────────────────────────────────

def open_csv(path: str):
    f = open(path, "w", newline="")
    writer = csv.DictWriter(f, fieldnames=CSV_HEADER)
    writer.writeheader()
    return f, writer


def write_row(writer, ts: str, entry: dict):
    writer.writerow({
        "timestamp":    ts,
        "node_id":      entry["node_id"],
        "latitude":     f"{entry['latitude']:.7f}",
        "longitude":    f"{entry['longitude']:.7f}",
        "lifetime":     entry["lifetime"],
        "P_Signal":     f"{entry['P_Signal']:.4f}",
        "P_Channel":    f"{entry['P_Channel']:.4f}",
        "switch_state": entry["switch_state"],
        "Dist":         f"{entry['Dist']:.4f}",
        "Heading":      f"{entry['Heading']:.4f}" if entry["Heading"] is not None else "",
    })


# ── Pretty printer ───────────────────────────────────────────────────────────

def pretty_print(ts: str, count: int, e: dict):
    heading_str = f"{e['Heading']:.2f}°" if e["Heading"] is not None else "N/A"
    print(
        f"┌─ Packet #{count}  [{ts}]\n"
        f"│  Node ID   : {e['node_id']}\n"
        f"│  Position  : {e['latitude']:.6f}, {e['longitude']:.6f}\n"
        f"│  Lifetime  : {e['lifetime']}\n"
        f"│  P_Signal  : {e['P_Signal']:.4f}\n"
        f"│  P_Channel : {e['P_Channel']:.4f}\n"
        f"│  Antenna   : {e['switch_state']}\n"
        f"│  Distance  : {e['Dist']:.4f} m\n"
        f"└  Heading   : {heading_str}"
    )


# ── CLI ───────────────────────────────────────────────────────────────────────

def main():
    parser = argparse.ArgumentParser(
        description="Decode ESP UART channel-state-table packets → CSV"
    )
    source = parser.add_mutually_exclusive_group(required=True)
    source.add_argument("--port",  "-p", help="Serial port, e.g. COM3 or /dev/ttyUSB0")
    source.add_argument("--file",  "-f", help="Raw binary file recorded from UART")

    parser.add_argument("--baud",  "-b", type=int, default=115200,
                        help="Baud rate (default: 115200)")
    parser.add_argument("--output", "-o", default=None,
                        help="Output CSV path (default: uart_log_<timestamp>.csv)")
    parser.add_argument("--quiet", "-q", action="store_true",
                        help="Suppress decoded packet output to stdout")

    args = parser.parse_args()

    # Build output filename
    if args.output is None:
        ts_str = datetime.now().strftime("%Y%m%d_%H%M%S")
        args.output = f"uart_log_{ts_str}.csv"

    csv_path = Path(args.output)
    csv_file, csv_writer = open_csv(str(csv_path))
    print(f"[INFO] Logging to {csv_path}", file=sys.stderr)

    packets_received = 0
    packets_decoded  = 0

    try:
        if args.file:
            # ── File mode ──────────────────────────────────────────────────
            raw = Path(args.file).read_bytes()
            print(f"[INFO] Reading {len(raw)} bytes from {args.file}", file=sys.stderr)
            for payload in read_packets_from_bytes(raw):
                packets_received += 1
                ts = datetime.now().isoformat(timespec="milliseconds")
                entry = parse_packet(payload)
                if entry:
                    packets_decoded += 1
                    write_row(csv_writer, ts, entry)
                    if not args.quiet:
                        pretty_print(ts, packets_decoded, entry)

        else:
            # ── Serial mode ────────────────────────────────────────────────
            print(f"[INFO] Opening {args.port} @ {args.baud} baud …", file=sys.stderr)
            with serial.Serial(args.port, args.baud, timeout=0.1) as ser:
                print("[INFO] Listening — press Ctrl+C to stop", file=sys.stderr)
                for payload in read_packets(ser):
                    packets_received += 1
                    ts = datetime.now().isoformat(timespec="milliseconds")
                    entry = parse_packet(payload)
                    if entry:
                        packets_decoded += 1
                        csv_file.flush()          # keep file readable while running
                        write_row(csv_writer, ts, entry)
                        if not args.quiet:
                            pretty_print(ts, packets_decoded, entry)

    except KeyboardInterrupt:
        print("\n[INFO] Interrupted by user", file=sys.stderr)
    finally:
        csv_file.close()
        print(f"[INFO] Done. Received {packets_received} packets, "
              f"decoded {packets_decoded} successfully.", file=sys.stderr)
        print(f"[INFO] CSV saved to: {csv_path}", file=sys.stderr)


if __name__ == "__main__":
    main()