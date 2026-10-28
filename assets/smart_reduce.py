#!/usr/bin/env python3
import argparse
import math
from collections import Counter
from PIL import Image

class GreedyColorReducer:
    def __init__(self, input_path, output_path, max_colors=15, threshold=30):
        self.input_path = input_path
        self.output_path = output_path
        self.max_colors = max_colors
        self.threshold = threshold # آستانه اختلاف رنگ (هرچه بیشتر، ادغام بیشتر)
        self.img = None
        self.pixels = []
        self.final_palette = []

    def load_image(self):
        print(f"Loading {self.input_path}...")
        self.img = Image.open(self.input_path).convert("RGBA")
        self.pixels = list(self.img.getdata())

    def color_distance(self, c1, c2):
        # محاسبه فاصله رنگی بین دو رنگ
        return math.sqrt((c1[0]-c2[0])**2 + (c1[1]-c2[1])**2 + (c1[2]-c2[2])**2)

    def merge_colors(self):
        # جدا کردن پیکسل‌های غیرشفاف
        opaque_pixels = [p for p in self.pixels if p[3] >= 128]
        color_counts = Counter(opaque_pixels)
        
        print(f"Original unique colors: {len(color_counts)}")
        
        # مرتب سازی رنگ‌ها از پرتکرارترین به کم‌تکرارترین
        sorted_colors = color_counts.most_common()
        
        # لیست نهایی پالت (ایندکس 0 همیشه شفاف است)
        self.final_palette = [(0, 0, 0, 0)]
        
        for color, count in sorted_colors:
            # اگر به حداکثر تعداد رنگ رسیدیم، بقیه را به نزدیک‌ترین رنگ موجود تبدیل می‌کنیم
            if len(self.final_palette) - 1 >= self.max_colors:
                break
                
            # آیا این رنگ به اندازه کافی به رنگ‌های موجود در پالت نزدیک است؟
            is_duplicate = False
            for existing_color in self.final_palette[1:]:
                if self.color_distance(color, existing_color) < self.threshold:
                    is_duplicate = True
                    break
            
            # اگر رنگ جدید و متمایزی است، به پالت اضافه کن
            if not is_duplicate:
                self.final_palette.append(color)

        print(f"Reduced to {len(self.final_palette) - 1} opaque colors + 1 transparent.")

    def apply_palette(self):
        print("Mapping original pixels to new palette...")
        new_pixels = []
        
        for p in self.pixels:
            if p[3] < 128:
                new_pixels.append((0, 0, 0, 0)) # Transparent
            else:
                # پیدا کردن نزدیک‌ترین رنگ در پالت نهایی
                best_dist = float('inf')
                best_color = self.final_palette[1]
                for c in self.final_palette[1:]:
                    dist = self.color_distance(p, c)
                    if dist < best_dist:
                        best_dist = dist
                        best_color = c
                new_pixels.append(best_color)
                
        self.img.putdata(new_pixels)
        self.img.save(self.output_path)
        
        print(f"Saved to {self.output_path}")
        print("--- Final Palette ---")
        for i, c in enumerate(self.final_palette):
            print(f"  Index {i:2}: RGB{c[:3]} {'(Transparent)' if i==0 else ''}")

    def run(self):
        self.load_image()
        self.merge_colors()
        self.apply_palette()

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Greedy Color Reducer for GBA")
    parser.add_argument("input", help="Input PNG file")
    parser.add_argument("-o", "--output", required=True, help="Output PNG file")
    parser.add_argument("-m", "--max-colors", type=int, default=15, help="Max opaque colors")
    parser.add_argument("-t", "--threshold", type=int, default=30, help="Color merge threshold (higher = more merging)")
    
    args = parser.parse_args()
    
    reducer = GreedyColorReducer(args.input, args.output, args.max_colors, args.threshold)
    reducer.run()
