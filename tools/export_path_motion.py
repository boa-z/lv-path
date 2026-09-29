#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Copyright (c) 2026 boa-z
"""Optional host visualization of actual path_motion CSV output (Pillow required).

Runs the compiled C example without a shell. No path math is reimplemented here;
every displayed position and direction comes from the executable's output.
Neither Python nor Pillow is a build/runtime dependency of the geometry core.
"""
import argparse
import csv
import io
import math
from pathlib import Path
import subprocess

from PIL import Image, ImageDraw, ImageFont

COLORS = {
    "bg": "#101827", "panel": "#192539", "grid": "#27384E",
    "path": "#536C8B", "ink": "#EBF3FF", "muted": "#99B0CC",
    "motion": "#57DDB8", "tangent": "#6CBBFF", "normal": "#F8BD72",
}
SIZE = (900, 560)


def read_motion(executable):
    run = subprocess.run([str(executable.resolve())], check=True, capture_output=True,
                         text=True, encoding="utf-8", timeout=30)
    reader = csv.DictReader(io.StringIO(run.stdout))
    expected = ["time_s", "requested_distance", "x", "y", "tangent_x", "tangent_y",
                "normal_x", "normal_y"]
    if reader.fieldnames != expected:
        raise ValueError("unexpected path_motion CSV columns")
    rows = [{key: float(row[key]) for key in expected} for row in reader]
    if len(rows) != 121:
        raise ValueError("expected the complete 121-row motion example")
    for i, row in enumerate(rows):
        if not all(math.isfinite(v) for v in row.values()):
            raise ValueError("nonfinite sample")
        if abs(row["time_s"] - i / 60) > 1e-5:
            raise ValueError("unexpected motion timestamps")
        if i and row["requested_distance"] <= rows[i-1]["requested_distance"]:
            raise ValueError("distance must increase")
        if abs(math.hypot(row["tangent_x"], row["tangent_y"]) - 1) > 2e-6:
            raise ValueError("non-unit tangent")
        if abs(row["normal_x"] + row["tangent_y"]) > 2e-6 or \
                abs(row["normal_y"] - row["tangent_x"]) > 2e-6:
            raise ValueError("invalid normal")
    if abs(rows[0]["requested_distance"]) > 1e-6 or \
            math.hypot(rows[0]["x"], rows[0]["y"]) > 1e-5 or \
            math.hypot(rows[-1]["x"] - 120, rows[-1]["y"]) > 1e-5:
        raise ValueError("unexpected example endpoints")
    return rows, run.stdout


def screen(row):
    return 100 + 4.5 * row["x"], 400 - 4.5 * row["y"]


def render(rows, index):
    frame = Image.new("RGB", SIZE, COLORS["bg"])
    draw = ImageDraw.Draw(frame)
    fonts = {n: ImageFont.load_default(size=n) for n in (14, 16, 20, 30)}

    def text(xy, value, size=16, color="ink"):
        draw.text(xy, value, font=fonts[size], fill=COLORS[color])

    def arrow(origin, vx, vy, length, color):
        x, y = origin
        vx, vy = vx * length, -vy * length
        end = (x + vx, y + vy)
        draw.line([origin, end], fill=COLORS[color], width=3)
        scale = math.hypot(vx, vy)
        ux, uy = vx / scale, vy / scale
        draw.polygon([end, (end[0]-8*ux+4*uy, end[1]-8*uy-4*ux),
                      (end[0]-8*ux-4*uy, end[1]-8*uy+4*ux)], fill=COLORS[color])

    text((32, 24), "lv-path / distance-driven motion", 30)
    text((34, 66), "C99 geometry  |  caller-owned storage  |  no heap in core", color="muted")
    draw.rounded_rectangle((700, 105, 866, 432), radius=14, fill=COLORS["panel"])
    for x in range(100, 641, 90):
        draw.line((x, 116, x, 416), fill=COLORS["grid"])
    for y in (130, 220, 310, 400):
        draw.line((80, y, 660, y), fill=COLORS["grid"])
    points = [screen(row) for row in rows]
    draw.line(points, fill=COLORS["path"], width=4)
    if index:
        draw.line(points[:index+1], fill=COLORS["motion"], width=4)
    for i in range(0, 121, 15):
        x, y = points[i]
        draw.ellipse((x-4, y-4, x+4, y+4), fill=COLORS["muted"])
    row = rows[index]
    x, y = points[index]
    arrow((x,y), row["tangent_x"], row["tangent_y"], 42, "tangent")
    arrow((x,y), row["normal_x"], row["normal_y"], 32, "normal")
    draw.ellipse((x-9, y-9, x+9, y+9), fill=COLORS["motion"], outline=COLORS["ink"], width=2)
    text((77, 423), "(0, 0)", 14, "muted")
    text((611, 423), "(120, 0)", 14, "muted")
    text((718, 123), "LIVE QUERY", 16, "muted")
    text((718, 159), f't = {row["time_s"]:.2f} s', 20)
    text((718, 196), f'd = {row["requested_distance"]:.2f}', 20)
    text((718, 241), f'x = {row["x"]:.2f}', 16)
    text((718, 265), f'y = {row["y"]:.2f}', 16)
    for j, (color, label) in enumerate((("motion","Position"),("tangent","Tangent"),("normal","Normal"))):
        yy = 318 + 29*j
        draw.line((719,yy+7,737,yy+7), fill=COLORS[color], width=3)
        text((746,yy),label,16,color)
    draw.rounded_rectangle((34, 467, 866, 475), radius=4, fill=COLORS["grid"])
    if index:
        draw.rounded_rectangle((34,467,34+832*index/120,475), radius=4, fill=COLORS["motion"])
    text((34, 493), "2-second trajectory | sample markers every 0.25 s | tolerance 0.1 path units", 16)
    text((34, 523), "Actual C example output. Host visualization; approximation + float limits apply.", 14, "muted")
    return frame


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--executable", type=Path, required=True)
    parser.add_argument("--output", type=Path, default=Path("docs/media/path-motion.gif"))
    args = parser.parse_args()
    rows, csv_text = read_motion(args.executable)
    frames = [render(rows, index) for index in range(0, 121, 2)]
    # GIF stores centiseconds: 30/40 ms delays preserve the 2 s motion interval.
    ticks = [round(i * 200 / 60) for i in range(61)]
    durations = [(ticks[i+1]-ticks[i])*10 for i in range(60)] + [600]
    durations[0] += 400  # labelled trajectory plus endpoint viewing pauses
    palette = Image.new("RGB", (SIZE[0]*3, SIZE[1]))
    for i, index in enumerate((0, 30, 60)):
        palette.paste(frames[index], (SIZE[0]*i, 0))
    palette = palette.quantize(colors=128)
    indexed = [frame.quantize(palette=palette, dither=Image.Dither.NONE) for frame in frames]
    args.output.parent.mkdir(parents=True, exist_ok=True)
    indexed[0].save(args.output, save_all=True, append_images=indexed[1:],
                    duration=durations, loop=0, optimize=False, disposal=2)
    csv_path = args.output.with_suffix(".csv")
    csv_path.write_text(csv_text, encoding="utf-8", newline="")
    with Image.open(args.output) as gif:
        delays = []
        for index in range(gif.n_frames):
            gif.seek(index)
            delays.append(gif.info["duration"])
        if gif.n_frames != 61 or sum(delays) != 3000:
            raise ValueError("GIF frame/timing validation failed")
    print(f"Created {args.output}: 61 frames, 900x560, 3000 ms loop (2000 ms motion)")
    print(f"Source samples: {csv_path} (121 rows)")


if __name__ == "__main__":
    main()
