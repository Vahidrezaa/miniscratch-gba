#!/usr/bin/env python3
import sys
import os
import argparse
import struct
from PIL import Image

class GBAConverter:
    def __init__(self, input_file, output_name, bpp=4, mode='sprite', 
                 compress='none', format='c', global_pal=None):
        self.input_file = input_file
        self.output_name = output_name
        self.bpp = bpp
        self.mode = mode
        self.compress = compress
        self.format = format
        self.max_colors = 1 << bpp if bpp < 16 else 0
        
        self.palette = [(0, 0, 0, 0)] # Index 0 = transparent
        self.palette_map = {}
        self.tiles = []
        self.tile_map = {}
        self.screenblock = []
        self.raw_pixels = bytearray()
        
        self.img = None
        self.width = 0
        self.height = 0
        self.global_pal = global_pal

    def load_and_process(self):
        self.img = Image.open(self.input_file).convert("RGBA")
        w, h = self.img.size
        new_w, new_h = (w + 7) // 8 * 8, (h + 7) // 8 * 8
        
        if (w, h) != (new_w, new_h):
            self.img = self.img.resize((new_w, new_h), Image.Resampling.NEAREST)
            
        self.width, self.height = self.img.size
        self._build_palette()

    def _build_palette(self):
        if self.global_pal:
            self.palette = self.global_pal
            self.palette_map = {c: i for i, c in enumerate(self.palette) if c[3] >= 128}
            return

        colors = set()
        for y in range(self.height):
            for x in range(self.width):
                r, g, b, a = self.img.getpixel((x, y))
                if a >= 128: colors.add((r, g, b))
                
        if len(colors) > self.max_colors - 1:
            print(f"Quantizing {len(colors)} colors to {self.max_colors-1}...")
            # حفظ کانال آلفا (شفافیت) در هنگام کاهش رنگ
            alpha = self.img.getchannel('A')
            self.img = self.img.convert('RGB').quantize(colors=self.max_colors-1).convert('RGBA')
            self.img.putalpha(alpha)

        for y in range(self.height):
            for x in range(self.width):
                r, g, b, a = self.img.getpixel((x, y))
                if a < 128: continue
                color = (r, g, b)
                if color not in self.palette_map:
                    self.palette_map[color] = len(self.palette)
                    # FIX: ذخیره به صورت 4-tuple
                    self.palette.append((r, g, b, 255))
                    
        while len(self.palette) < self.max_colors:
            self.palette.append((0, 0, 0, 0))

    def _get_idx(self, x, y):
        r, g, b, a = self.img.getpixel((x, y))
        return 0 if a < 128 else self.palette_map.get((r, g, b), 1)

    def _flip_h(self, tile_data):
        flipped = []
        for y in range(8):
            row = tile_data[y*4 : (y+1)*4]
            flipped.extend([((byte & 0x0F) << 4) | ((byte & 0xF0) >> 4) for byte in reversed(row)])
        return bytes(flipped)

    def _flip_v(self, tile_data):
        return b''.join([tile_data[y*4 : (y+1)*4] for y in range(7, -1, -1)])

    def _pack_tile(self, tx, ty):
        tile_bytes = bytearray()
        if self.bpp == 16:
            for y in range(8):
                for x in range(8):
                    r, g, b, a = self.img.getpixel((tx+x, ty+y))
                    val = 0 if a < 128 else ((b>>3)<<10)|((g>>3)<<5)|(r>>3)
                    tile_bytes.extend(struct.pack('<H', val))
            return bytes(tile_bytes)

        for y in range(8):
            for x in range(0, 8, 8 // self.bpp):
                if self.bpp == 8:
                    tile_bytes.append(self._get_idx(tx+x, ty+y))
                elif self.bpp == 4:
                    i1 = self._get_idx(tx+x, ty+y)
                    i2 = self._get_idx(tx+x+1, ty+y)
                    tile_bytes.append((i2 << 4) | i1)
                elif self.bpp == 2:
                    i1 = self._get_idx(tx+x, ty+y)
                    i2 = self._get_idx(tx+x+1, ty+y)
                    i3 = self._get_idx(tx+x+2, ty+y)
                    i4 = self._get_idx(tx+x+3, ty+y)
                    tile_bytes.append((i4 << 6) | (i3 << 4) | (i2 << 2) | i1)
                elif self.bpp == 1:
                    byte = 0
                    for i in range(8):
                        byte |= (self._get_idx(tx+x+i, ty+y) & 1) << i
                    tile_bytes.append(byte)
        return bytes(tile_bytes)

    def build_tiles_and_map(self):
        if self.bpp == 16:
            print("Building 16bpp raw pixels...")
            for y in range(self.height):
                for x in range(self.width):
                    r, g, b, a = self.img.getpixel((x, y))
                    val = 0 if a < 128 else ((b>>3)<<10)|((g>>3)<<5)|(r>>3)
                    self.raw_pixels.extend(struct.pack('<H', val))
            return

        print("Building tiles and screenblock...")
        for ty in range(0, self.height, 8):
            for tx in range(0, self.width, 8):
                t_bytes = self._pack_tile(tx, ty)
                
                if t_bytes in self.tile_map:
                    idx, hf, vf = self.tile_map[t_bytes]
                else:
                    h_flip = self._flip_h(t_bytes)
                    v_flip = self._flip_v(t_bytes)
                    hv_flip = self._flip_v(h_flip)
                    
                    if h_flip in self.tile_map:
                        idx, _, _ = self.tile_map[h_flip]; hf, vf = 1, 0
                    elif v_flip in self.tile_map:
                        idx, _, _ = self.tile_map[v_flip]; hf, vf = 0, 1
                    elif hv_flip in self.tile_map:
                        idx, _, _ = self.tile_map[hv_flip]; hf, vf = 1, 1
                    else:
                        idx = len(self.tiles)
                        self.tiles.append(t_bytes)
                        self.tile_map[t_bytes] = (idx, 0, 0)
                        hf, vf = 0, 0
                
                entry = idx | (hf << 10) | (vf << 11)
                self.screenblock.append(entry)

    def _compress_lz77(self, data):
        print("Compressing with LZ77...")
        compressed = bytearray([0x10, len(data) & 0xFF, (len(data)>>8)&0xFF, (len(data)>>16)&0xFF])
        i = 0
        while i < len(data):
            flags = 0
            block = bytearray()
            for j in range(8):
                if i >= len(data): break
                best_len = 0
                best_disp = 0
                if i > 0:
                    for disp in range(1, min(4096, i) + 1):
                        length = 0
                        while length < 18 and i + length < len(data) and data[i+length] == data[i-disp+length]:
                            length += 1
                        if length > best_len:
                            best_len = length
                            best_disp = disp
                
                if best_len >= 3:
                    flags |= (1 << j)
                    token = (best_len - 3) << 12 | (best_disp - 1)
                    block.extend(struct.pack('>H', token))
                    i += best_len
                else:
                    block.append(data[i])
                    i += 1
            compressed.append(flags)
            compressed.extend(block)
        return bytes(compressed)

    def _compress_rle(self, data):
        print("Compressing with RLE...")
        compressed = bytearray([0x30, len(data) & 0xFF, (len(data)>>8)&0xFF, (len(data)>>16)&0xFF])
        i = 0
        while i < len(data):
            flags = 0
            block = bytearray()
            for j in range(8):
                if i >= len(data): break
                run_len = 1
                while i + run_len < len(data) and data[i] == data[i+run_len] and run_len < 130:
                    run_len += 1
                
                if run_len >= 3:
                    flags |= (1 << j)
                    block.extend([run_len - 3, data[i]])
                    i += run_len
                else:
                    block.append(data[i])
                    i += 1
            compressed.append(flags)
            compressed.extend(block)
        return bytes(compressed)

    def _to_rgb555(self, r, g, b):
        return ((b >> 3) << 10) | ((g >> 3) << 5) | (r >> 3)

    def export(self):
        print(f"Exporting {self.output_name} in {self.format.upper()} format...")
        
        data_tiles = self.raw_pixels if self.bpp == 16 else b''.join(self.tiles)
        # FIX: استفاده ایمن از r,g,b,a
        data_pal = b''.join([struct.pack('<H', self._to_rgb555(item[0], item[1], item[2]) if item[3]>=128 else 0) for item in self.palette])
        data_map = b''.join([struct.pack('<H', e) for e in self.screenblock])

        if self.compress == 'lz77':
            data_tiles = self._compress_lz77(data_tiles)
            data_map = self._compress_lz77(data_map)
        elif self.compress == 'rle':
            data_tiles = self._compress_rle(data_tiles)
            data_map = self._compress_rle(data_map)
        while len(data_tiles) % 4 != 0:
            data_tiles += b'\x00'
        while len(data_map) % 2 != 0:
            data_map += b'\x00'

        if self.format == 'bin':
            with open(f"{self.output_name}_tiles.bin", 'wb') as f: f.write(data_tiles)
            with open(f"{self.output_name}_pal.bin", 'wb') as f: f.write(data_pal)
            if self.mode == 'bg':
                with open(f"{self.output_name}_map.bin", 'wb') as f: f.write(data_map)
            return

        if self.format == 'asm':
            with open(f"{self.output_name}.s", 'w') as f:
                f.write(f"@ Generated by png2gba\n.section .rodata\n\n")
                f.write(f".global {self.output_name}Tiles\n{self.output_name}Tiles:\n")
                for i in range(0, len(data_tiles), 4):
                    f.write(f"    .word 0x{struct.unpack('<I', data_tiles[i:i+4].ljust(4, b'\x00'))[0]:08X}\n")
                f.write(f"\n.global {self.output_name}Pal\n{self.output_name}Pal:\n")
                for i in range(0, len(data_pal), 2):
                    f.write(f"    .hword 0x{struct.unpack('<H', data_pal[i:i+2])[0]:04X}\n")
                if self.mode == 'bg':
                    f.write(f"\n.global {self.output_name}Map\n{self.output_name}Map:\n")
                    for i in range(0, len(data_map), 2):
                        f.write(f"    .hword 0x{struct.unpack('<H', data_map[i:i+2])[0]:04X}\n")
            return

        with open(f"{self.output_name}.h", 'w') as f:
            f.write(f"#ifndef {self.output_name.upper()}_H\n#define {self.output_name.upper()}_H\n\n")
            f.write(f"#define {self.output_name.upper()}_WIDTH {self.width}\n")
            f.write(f"#define {self.output_name.upper()}_HEIGHT {self.height}\n\n")
            dtype = 'unsigned short' if self.bpp == 16 else 'unsigned int'
            f.write(f"extern const {dtype} {self.output_name}Tiles[{len(data_tiles)//(2 if self.bpp==16 else 4)}];\n")
            f.write(f"extern const unsigned short {self.output_name}Pal[{len(self.palette)}];\n")
            if self.mode == 'bg':
                f.write(f"extern const unsigned short {self.output_name}Map[{len(self.screenblock)}];\n")
            f.write(f"\n#endif\n")

        with open(f"{self.output_name}.c", 'w') as f:
            f.write(f'#include "{self.output_name}.h"\n\n')
            
            dtype = 'unsigned short' if self.bpp == 16 else 'unsigned int'
            f.write(f"const {dtype} {self.output_name}Tiles[{len(data_tiles)//(2 if self.bpp==16 else 4)}] = {{\n")
            step = 2 if self.bpp == 16 else 4
            fmt = 'H' if self.bpp == 16 else 'I'
            for i in range(0, len(data_tiles), step):
                chunk = data_tiles[i:i+step].ljust(step, b'\x00')
                f.write(f"    0x{struct.unpack('<' + fmt, chunk)[0]:0{step*2}X},\n")
            f.write("};\n\n")

            f.write(f"const unsigned short {self.output_name}Pal[{len(self.palette)}] = {{\n")
            for i in range(0, len(data_pal), 2):
                f.write(f"    0x{struct.unpack('<H', data_pal[i:i+2])[0]:04X},\n")
            f.write("};\n\n")

            if self.mode == 'bg':
                f.write(f"const unsigned short {self.output_name}Map[{len(self.screenblock)}] = {{\n")
                for i in range(0, len(data_map), 2):
                    f.write(f"    0x{struct.unpack('<H', data_map[i:i+2])[0]:04X},\n")
                f.write("};\n")

def parse_hex_color(hex_str):
    h = hex_str.lstrip('#')
    return tuple(int(h[i:i+2], 16) for i in (0, 2, 4)) + (255,)

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="GBA Image Converter")
    parser.add_argument("input", help="Input PNG file")
    parser.add_argument("-o", "--output", required=True, help="Output base name")
    parser.add_argument("-b", "--bpp", type=int, choices=[1, 2, 4, 8, 16], default=4, help="Bits per pixel")
    parser.add_argument("--bg", action="store_true", help="Generate BG Map")
    parser.add_argument("-c", "--compress", choices=['none', 'lz77', 'rle'], default='none', help="Compression algorithm")
    parser.add_argument("-f", "--format", choices=['c', 'bin', 'asm'], default='c', help="Output format")
    parser.add_argument("-p", "--palette", help="Comma separated hex colors for global palette (e.g. '000000,FF0000')")
    
    args = parser.parse_args()
    
    global_pal = None
    if args.palette:
        global_pal = [(0,0,0,0)] + [parse_hex_color(c) for c in args.palette.split(',')]
    
    conv = GBAConverter(args.input, args.output, bpp=args.bpp, mode='bg' if args.bg else 'sprite',
                        compress=args.compress, format=args.format, global_pal=global_pal)
    conv.load_and_process()
    conv.build_tiles_and_map()
    conv.export()
    print("Success! Conversion complete.")
