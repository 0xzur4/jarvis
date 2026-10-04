// pixel_face.cpp
#include "pixel_face.h"
#include "faces.h"

namespace {
struct FaceDef {
    FaceId id;
    int w, h;
    const uint8_t* bits;
};
const FaceDef kDefs[] = {
    {FaceId::IDLE,        FACE_IDLE_W,        FACE_IDLE_H,        FACE_IDLE_BITS},
    {FaceId::TALK_OPEN,   FACE_TALK_OPEN_W,   FACE_TALK_OPEN_H,   FACE_TALK_OPEN_BITS},
    {FaceId::TALK_CLOSED, FACE_TALK_CLOSED_W, FACE_TALK_CLOSED_H, FACE_TALK_CLOSED_BITS},
    {FaceId::LISTEN,      FACE_LISTEN_W,      FACE_LISTEN_H,      FACE_LISTEN_BITS},
    {FaceId::THINK,       FACE_THINK_W,       FACE_THINK_H,       FACE_THINK_BITS},
};
}  // namespace

bool PixelFace::init() {
    for (const auto& d : kDefs) {
        FaceBmp& f = faces_[(int)d.id];
        f.w = d.w; f.h = d.h;
        f.px.assign((size_t)d.w * d.h, 0);
        int stride = (d.w + 7) / 8;
        for (int y = 0; y < d.h; ++y) {
            for (int x = 0; x < d.w; ++x) {
                uint8_t byte = d.bits[y * stride + x / 8];
                bool white = (byte >> (7 - (x % 8))) & 1;
                // Source PNG: black line-art on white. We want white lines,
                // transparent background so the dark window shows through.
                f.px[(size_t)y * d.w + x] = white ? 0x00000000u : 0xFFFFFFFFu;
            }
        }
    }
    return true;
}

void PixelFace::draw(HDC hdc, const RECT& rc, FaceId face, int scale) {
    const FaceBmp& f = faces_[(int)face];
    if (f.w == 0) return;
    int dw = f.w * scale, dh = f.h * scale;
    int dx = rc.left + ((rc.right - rc.left) - dw) / 2;
    int dy = rc.top + ((rc.bottom - rc.top) - dh) / 2;

    BITMAPINFO bmi{};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = f.w;
    bmi.bmiHeader.biHeight = -f.h;  // top-down
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    void* bits = nullptr;
    HDC mem = CreateCompatibleDC(hdc);
    HBITMAP hbmp = CreateDIBSection(mem, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (!hbmp) { DeleteDC(mem); return; }
    memcpy(bits, f.px.data(), f.px.size() * sizeof(uint32_t));
    HBITMAP old = (HBITMAP)SelectObject(mem, hbmp);

    BLENDFUNCTION bf{};
    bf.BlendOp = AC_SRC_OVER;
    bf.SourceConstantAlpha = 255;
    bf.AlphaFormat = AC_SRC_ALPHA;
    AlphaBlend(hdc, dx, dy, dw, dh, mem, 0, 0, f.w, f.h, bf);

    SelectObject(mem, old);
    DeleteObject(hbmp);
    DeleteDC(mem);
}

bool PixelFace::size(FaceId face, int& w, int& h) const {
    const FaceBmp& f = faces_[(int)face];
    if (f.w == 0) return false;
    w = f.w; h = f.h;
    return true;
}
