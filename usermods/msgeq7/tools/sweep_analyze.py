#!/usr/bin/env python3
"""
sweep_analyze.py — MSGEQ7 usermod band-response analyser.

Reads a serial log produced while a sine sweep (low → high frequency) is
played through the microphone, parses the MSGEQ7 band debug lines, and plots
band amplitudes over time to verify the filter chain is working correctly.

Expected serial log format (emitted when SR_DEBUG is defined):
    MSGEQ7 bands: 0=NN 1=NN 2=NN 3=NN 4=NN 5=NN 6=NN

Usage:
    python3 sweep_analyze.py <serial_log.txt>

Requirements:
    pip install matplotlib numpy

Output:
    - A stacked band-amplitude plot (bands 0-6 vs. time).
    - A summary of which band peaked when, and whether the peak order matches
      the expected low-to-high sweep sequence.

If matplotlib is not available the script still prints a text-mode summary.
"""

import re
import sys
from pathlib import Path

# MSGEQ7 center frequencies for axis labels
MSGEQ7_FREQS = [63, 160, 400, 1000, 2500, 6250, 16000]
NUM_BANDS = 7

# Regex matching the debug line printed by softwareProcessingTask when
# MSGEQ7_DEBUG_PRINT is defined, e.g.:
#   MSGEQ7 bands: 0=127 1=43 2=8 3=2 4=1 5=0 6=0
_LINE_RE = re.compile(
    r"MSGEQ7 bands:\s+" + r"\s+".join(rf"{b}=(\d+)" for b in range(NUM_BANDS))
)


def parse_log(path: Path) -> list[list[int]]:
    """Return a list of frames, each frame being a list of 7 band values."""
    frames = []
    with path.open(errors="replace") as fh:
        for line in fh:
            m = _LINE_RE.search(line)
            if m:
                frames.append([int(m.group(b + 1)) for b in range(NUM_BANDS)])
    return frames


def peak_order_check(frames: list[list[int]]) -> None:
    """
    Determine the time index at which each band reached its maximum amplitude
    and check whether the order is monotonically increasing (band 0 peaks
    first, band 6 peaks last), as expected for a low-to-high sine sweep.
    """
    if not frames:
        print("No data frames found. Check log format.")
        return

    n = len(frames)
    peak_times = []
    for b in range(NUM_BANDS):
        col = [frames[i][b] for i in range(n)]
        peak_idx = col.index(max(col))
        peak_val = col[peak_idx]
        peak_times.append((peak_idx, peak_val, b))
        print(f"  Band {b} ({MSGEQ7_FREQS[b]:>6} Hz): peak={peak_val:>3}  "
              f"at frame {peak_idx:>4} / {n}")

    ordered = all(
        peak_times[i][0] <= peak_times[i + 1][0] for i in range(NUM_BANDS - 1)
    )
    print()
    if ordered:
        print("PASS: bands peaked in correct low→high order.")
    else:
        print("FAIL: band peak order does not match expected low→high sequence.")
        print("      (This is normal for music; expected only for a sine sweep test.)")


def plot(frames: list[list[int]]) -> None:
    try:
        import matplotlib.pyplot as plt  # type: ignore
        import numpy as np               # type: ignore
    except ImportError:
        print("matplotlib/numpy not available — skipping plot (text summary only).")
        return

    data = np.array(frames, dtype=float)  # shape (frames, 7)
    t = np.arange(len(frames))

    fig, axes = plt.subplots(NUM_BANDS, 1, figsize=(12, 10), sharex=True)
    fig.suptitle("MSGEQ7 Band Amplitudes vs. Time", fontsize=13)

    colors = plt.cm.plasma(np.linspace(0.1, 0.9, NUM_BANDS))

    for b, ax in enumerate(axes):
        ax.fill_between(t, data[:, b], alpha=0.7, color=colors[b])
        ax.set_ylabel(f"{MSGEQ7_FREQS[b]} Hz", fontsize=8, rotation=0,
                      labelpad=45, va="center")
        ax.set_ylim(0, 260)
        ax.set_yticks([0, 128, 255])
        ax.grid(axis="x", linestyle=":", alpha=0.4)

    axes[-1].set_xlabel("Frame index")
    plt.tight_layout()
    plt.show()


def main() -> int:
    if len(sys.argv) < 2:
        print(f"Usage: {sys.argv[0]} <serial_log.txt>")
        return 1

    path = Path(sys.argv[1])
    if not path.exists():
        print(f"File not found: {path}")
        return 1

    frames = parse_log(path)
    print(f"Parsed {len(frames)} band frames from {path}\n")

    if not frames:
        print("No 'MSGEQ7 bands:' lines found.")
        print("Make sure the firmware was built with -D SR_DEBUG and the log")
        print("was captured while audio was playing.")
        return 1

    peak_order_check(frames)
    plot(frames)
    return 0


if __name__ == "__main__":
    sys.exit(main())
