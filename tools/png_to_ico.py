from __future__ import annotations

import struct
import sys
import zlib
from pathlib import Path


PNG_SIGNATURE = b"\x89PNG\r\n\x1a\n"


def _read_png(path: Path) -> tuple[int, int, list[tuple[int, int, int, int]]]:
    data = path.read_bytes()
    if not data.startswith(PNG_SIGNATURE):
        raise ValueError(f"Not a PNG file: {path}")

    offset = len(PNG_SIGNATURE)
    width = height = 0
    bit_depth = color_type = interlace = None
    idat_parts: list[bytes] = []

    while offset + 8 <= len(data):
        length = int.from_bytes(data[offset:offset + 4], "big")
        chunk_type = data[offset + 4:offset + 8]
        chunk_data = data[offset + 8:offset + 8 + length]
        offset += 12 + length

        if chunk_type == b"IHDR":
            width = int.from_bytes(chunk_data[0:4], "big")
            height = int.from_bytes(chunk_data[4:8], "big")
            bit_depth = chunk_data[8]
            color_type = chunk_data[9]
            interlace = chunk_data[12]
        elif chunk_type == b"IDAT":
            idat_parts.append(chunk_data)
        elif chunk_type == b"IEND":
            break

    if width <= 0 or height <= 0:
        raise ValueError("PNG is missing a valid IHDR chunk")
    if bit_depth != 8 or color_type != 6:
        raise ValueError("Only 8-bit RGBA PNG files are currently supported")
    if interlace != 0:
        raise ValueError("Interlaced PNG files are not supported")

    compressed = b"".join(idat_parts)
    raw = zlib.decompress(compressed)

    bytes_per_pixel = 4
    stride = width * bytes_per_pixel
    expected_size = (stride + 1) * height
    if len(raw) != expected_size:
        raise ValueError("PNG pixel stream has an unexpected size")

    pixels: list[tuple[int, int, int, int]] = []
    previous_row = bytearray(stride)
    pos = 0

    for _ in range(height):
        filter_type = raw[pos]
        pos += 1
        row = bytearray(raw[pos:pos + stride])
        pos += stride

        if filter_type == 1:
            for i in range(stride):
                left = row[i - bytes_per_pixel] if i >= bytes_per_pixel else 0
                row[i] = (row[i] + left) & 0xFF
        elif filter_type == 2:
            for i in range(stride):
                row[i] = (row[i] + previous_row[i]) & 0xFF
        elif filter_type == 3:
            for i in range(stride):
                left = row[i - bytes_per_pixel] if i >= bytes_per_pixel else 0
                up = previous_row[i]
                row[i] = (row[i] + ((left + up) // 2)) & 0xFF
        elif filter_type == 4:
            for i in range(stride):
                left = row[i - bytes_per_pixel] if i >= bytes_per_pixel else 0
                up = previous_row[i]
                up_left = previous_row[i - bytes_per_pixel] if i >= bytes_per_pixel else 0
                prediction = left + up - up_left
                pa = abs(prediction - left)
                pb = abs(prediction - up)
                pc = abs(prediction - up_left)
                if pa <= pb and pa <= pc:
                    chosen = left
                elif pb <= pc:
                    chosen = up
                else:
                    chosen = up_left
                row[i] = (row[i] + chosen) & 0xFF
        elif filter_type != 0:
            raise ValueError(f"Unsupported PNG filter type: {filter_type}")

        for i in range(0, stride, 4):
            pixels.append((row[i], row[i + 1], row[i + 2], row[i + 3]))

        previous_row = row

    return width, height, pixels


def _resize_nearest(
    width: int,
    height: int,
    pixels: list[tuple[int, int, int, int]],
    target_size: int,
) -> tuple[int, int, list[tuple[int, int, int, int]]]:
    if width == target_size and height == target_size:
        return width, height, pixels

    out: list[tuple[int, int, int, int]] = []
    for y in range(target_size):
        src_y = min(height - 1, int(y * height / target_size))
        for x in range(target_size):
            src_x = min(width - 1, int(x * width / target_size))
            out.append(pixels[src_y * width + src_x])
    return target_size, target_size, out


def _build_ico_bytes(size: int, pixels: list[tuple[int, int, int, int]]) -> bytes:
    xor_stride = size * 4
    and_stride = ((size + 31) // 32) * 4
    and_mask = b"\x00" * (and_stride * size)

    dib = bytearray()
    dib.extend(struct.pack("<IIIHHIIIIII", 40, size, size * 2, 1, 32, 0, xor_stride * size + len(and_mask), 0, 0, 0, 0))

    for y in range(size - 1, -1, -1):
        row_offset = y * size
        for x in range(size):
            r, g, b, a = pixels[row_offset + x]
            dib.extend(bytes((b, g, r, a)))

    dib.extend(and_mask)

    header = struct.pack("<HHH", 0, 1, 1)
    entry = struct.pack(
        "<BBBBHHII",
        0 if size >= 256 else size,
        0 if size >= 256 else size,
        0,
        0,
        1,
        32,
        len(dib),
        6 + 16,
    )
    return header + entry + dib


def main(argv: list[str]) -> int:
    if len(argv) != 3:
        print("usage: png_to_ico.py <input.png> <output.ico>", file=sys.stderr)
        return 1

    source = Path(argv[1])
    target = Path(argv[2])

    width, height, pixels = _read_png(source)
    size = min(256, width, height)
    _, _, resized = _resize_nearest(width, height, pixels, size)
    target.write_bytes(_build_ico_bytes(size, resized))
    print(f"wrote {target}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv))
