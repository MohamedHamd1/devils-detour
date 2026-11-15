# Box art / preview for muOS. 1) build the test binary, 2) render the scene with the game's own sprites:
#    SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy ./out/trust_test --shot art 1 0 /tmp/scene_box.bmp    (480x640)
#    SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy ./out/trust_test --shot art 1 1 /tmp/scene_prev.bmp   (640x480)
# 3) python3 make_art.py  -> box.png (240x320) and preview.png (320x240); copy to
#    MUOS/info/catalogue/External - Ports/{box,preview}/Devils Detour.png
from PIL import Image, ImageDraw, ImageFont, ImageFilter
B = "/usr/share/fonts/truetype/liberation/LiberationSans-Bold.ttf"
def grad_text(img, text, cx, y, size, stroke=8):
    f = ImageFont.truetype(B, size); d = ImageDraw.Draw(img); w = d.textlength(text, font=f); x = cx - w / 2
    sh = Image.new('RGBA', img.size, (0, 0, 0, 0)); ImageDraw.Draw(sh).text((x + 5, y + 8), text, font=f, fill=(20, 0, 40, 200), stroke_width=stroke, stroke_fill=(20, 0, 40, 200))
    img.alpha_composite(sh.filter(ImageFilter.GaussianBlur(2)))
    d.text((x, y), text, font=f, fill=(255, 255, 255), stroke_width=stroke, stroke_fill=(48, 10, 84))
    mask = Image.new('L', img.size, 0); ImageDraw.Draw(mask).text((x, y), text, font=f, fill=255)
    g = Image.new('RGBA', img.size, (0, 0, 0, 0)); gd = ImageDraw.Draw(g); bb = f.getbbox(text); top = y + bb[1]; bot = y + bb[3]
    for yy in range(int(top), int(bot) + 1):
        t = (yy - top) / max(1, bot - top)
        c = (255, int(246 - 126 * t), int(120 - 50 * t)) if t < 0.55 else (255, int(160 - 70 * (t - 0.55) / 0.45), int(80 + 10 * (t - 0.55) / 0.45))
        gd.line([(0, yy), (img.size[0], yy)], fill=c + (255,))
    g.putalpha(mask); img.alpha_composite(g)
def sub(img, text, cx, y, size, fill=(255, 244, 200)):
    f = ImageFont.truetype(B, size); d = ImageDraw.Draw(img); w = d.textlength(text, font=f)
    d.text((cx - w / 2, y), text, font=f, fill=fill, stroke_width=4, stroke_fill=(40, 8, 70))
im = Image.open('/tmp/scene_box.bmp').convert('RGBA')
grad_text(im, "DEVIL'S", 240, 34, 100, 8); grad_text(im, "DETOUR", 240, 124, 100, 8); sub(im, "Every level lies to you.", 240, 252, 32)
im.convert('RGB').resize((240, 320), Image.LANCZOS).save('box.png')
im = Image.open('/tmp/scene_prev.bmp').convert('RGBA')
grad_text(im, "DEVIL'S DETOUR", 320, 16, 72, 8); sub(im, "Every level lies to you.", 320, 112, 30)
im.convert('RGB').resize((320, 240), Image.LANCZOS).save('preview.png')
