#!/usr/bin/env python3
"""
Decode a DDAQ binary log (LOGxxxxx.BIN) from the SD card.

Usage:
    python decode_log.py LOG00001.BIN            # print a summary
    python decode_log.py LOG00001.BIN -o run.csv # TODO(T9): write a CSV

File format (must match Core/Inc/logger.h):
    32-byte file header, then records of  [sync 0xA5][type u8][len u16][payload]
    type 0x01 = sample (40 bytes), 0x02 = CAN frame (20 bytes), 0x03 = event text
"""
import argparse
import struct
import sys

HEADER = struct.Struct("<8sHHBBBBI8sBB2s")          # log_file_header_t, 32 bytes
REC_HDR = struct.Struct("<BBH")                     # log_rec_hdr_t, 4 bytes
SAMPLE = struct.Struct("<I8H2H4I")                  # log_sample_t, 40 bytes
CAN = struct.Struct("<IIBBBB8s")                    # log_can_t, 20 bytes

REC_SYNC = 0xA5
REC_SAMPLE, REC_CAN, REC_EVENT = 0x01, 0x02, 0x03

# ADS8688 range code -> (volts per count, offset in counts)
RANGES = {
    0x00: (20.48 / 65536, 32768),   # +-10.24 V
    0x01: (10.24 / 65536, 32768),   # +-5.12 V
    0x02: (5.12 / 65536, 32768),    # +-2.56 V
    0x05: (10.24 / 65536, 0),       # 0..10.24 V
    0x06: (5.12 / 65536, 0),        # 0..5.12 V
}


def read_header(data):
    if len(data) < HEADER.size:
        raise ValueError("file is shorter than the 32-byte header")
    (magic, ver, rate, node, n_ai, n_aux, n_freq, tick_hz,
     ranges, fw_major, fw_minor, _) = HEADER.unpack_from(data, 0)
    if not magic.startswith(b"DDAQLOG"):
        raise ValueError(f"not a DDAQ log (magic {magic!r})")
    return {
        "format_version": ver, "sample_rate_hz": rate, "node_id": node,
        "n_ai": n_ai, "n_aux": n_aux, "n_freq": n_freq,
        "freq_tick_hz": tick_hz, "ai_range": list(ranges),
        "firmware": f"{fw_major}.{fw_minor}",
    }


def iter_records(data, start):
    """Yield (type, payload bytes). Skips forward to the next 0xA5 after corruption."""
    pos, skipped = start, 0
    while pos + REC_HDR.size <= len(data):
        sync, rtype, length = REC_HDR.unpack_from(data, pos)
        end = pos + REC_HDR.size + length
        if sync != REC_SYNC or end > len(data):
            pos += 1          # lost sync (or truncated last record): search forward
            skipped += 1
            continue
        yield rtype, data[pos + REC_HDR.size:end]
        pos = end
    if skipped:
        print(f"warning: skipped {skipped} bytes of corrupt or truncated data", file=sys.stderr)


def decode_sample(payload, header):
    v = SAMPLE.unpack(payload)
    t_ms, ai_raw, aux_raw, periods = v[0], v[1:9], v[9:11], v[11:15]
    ai_volts = []
    for ch, raw in enumerate(ai_raw):
        lsb, offset = RANGES.get(header["ai_range"][ch], (0.0, 0))
        ai_volts.append((raw - offset) * lsb)
    freq_hz = [header["freq_tick_hz"] / p if p else 0.0 for p in periods]
    return {"t_ms": t_ms, "ai_raw": ai_raw, "ai_volts": ai_volts,
            "aux_raw": aux_raw, "freq_hz": freq_hz}


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("log", help="LOGxxxxx.BIN from the SD card")
    ap.add_argument("-o", "--csv", help="write samples to this CSV file (T9)")
    args = ap.parse_args()

    with open(args.log, "rb") as f:
        data = f.read()

    header = read_header(data)
    counts = {REC_SAMPLE: 0, REC_CAN: 0, REC_EVENT: 0}
    first = last = None
    gaps = 0

    for rtype, payload in iter_records(data, HEADER.size):
        counts[rtype] = counts.get(rtype, 0) + 1
        if rtype == REC_SAMPLE and len(payload) == SAMPLE.size:
            s = decode_sample(payload, header)
            if last is not None and s["t_ms"] != last["t_ms"] + 1:
                gaps += 1     # a sample was dropped (ring buffer full) or the log was cut
            first = first or s
            last = s
        elif rtype == REC_CAN:
            # TODO(T7/T9): decode with CAN.unpack(payload) -> t_ms, id, bus, dlc, flags, _, data
            # and write the frames to a second CSV (e.g. run_can.csv).
            pass

    print(f"header: {header}")
    print(f"records: {counts}")
    if first:
        secs = (last["t_ms"] - first["t_ms"]) / header["sample_rate_hz"]
        print(f"samples: t_ms {first['t_ms']} .. {last['t_ms']}  ({secs:.1f} s), gaps: {gaps}")
        print("last sample volts:", ", ".join(f"{v:.3f}" for v in last["ai_volts"]))

    if args.csv:
        # TODO(T9): write one CSV row per sample:
        #   t_s, AI0_V .. AI7_V, AI8_raw, AI9_raw, FREQ1_Hz .. FREQ4_Hz
        # Use the csv module (csv.writer) and decode_sample() above.
        # Stretch: use the sensor names/units from sensors.c as column headers,
        # and add a --plot option with matplotlib.
        # Test: open the CSV in Excel; with nothing connected AI0..AI7 read ~0 V.
        print("CSV export is not written yet (task T9).", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
