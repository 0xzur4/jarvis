// pixel_face.h - renders the 1-bit pixel faces as white line-art on dark bg.
#pragma once
#include <windows.h>
#include <cstdint>
#include <vector>

enum class FaceId { IDLE = 0, TALK_OPEN, TALK_CLOSED, LISTEN, THINK, COUNT };

class PixelFace {
public:
    // Expand 1-bit bitmaps to 32bpp DIBs (white lines, transparent bg).
    bool init();
    // Draw face centered in rc at integer scale, using AlphaBlend.
    void draw(HDC hdc, const RECT& rc, FaceId face, int scale = 4);
    // Pixel dimensions of a face (valid after init()).
    bool size(FaceId face, int& w, int& h) const;
private:
    struct FaceBmp { int w = 0, h = 0; std::vector<uint32_t> px; };
    FaceBmp faces_[(int)FaceId::COUNT];
};
