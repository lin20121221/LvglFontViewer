#include "rledecompressor.h"
#include <QDebug>

enum RLEState {
    RLE_STATE_SINGLE,
    RLE_STATE_REPEATE,
    RLE_STATE_COUNTER
};

static RLEState rle_state;
static uint8_t rle_prev_v;
static uint8_t rle_cnt;

QVector<uint8_t> RLEDecompressor::decompress(BitReader &reader, int pixelCount, int bpp)
{
    QVector<uint8_t> pixels;
    pixels.reserve(pixelCount);

    rle_state = RLE_STATE_SINGLE;
    rle_prev_v = 0;
    rle_cnt = 0;

    for (int i = 0; i < pixelCount; i++) {
        uint8_t v = 0;
        uint8_t ret = 0;

        if (rle_state == RLE_STATE_SINGLE) {
            ret = reader.readBits(bpp);
            if (i != 0 && rle_prev_v == ret) {
                rle_cnt = 0;
                rle_state = RLE_STATE_REPEATE;
            }
            rle_prev_v = ret;
        }
        else if (rle_state == RLE_STATE_REPEATE) {
            v = reader.readBits(1);
            rle_cnt++;
            if (v == 1) {
                ret = rle_prev_v;
                if (rle_cnt == 11) {
                    rle_cnt = reader.readBits(6);
                    if (rle_cnt != 0) {
                        rle_state = RLE_STATE_COUNTER;
                    }
                    else {
                        ret = reader.readBits(bpp);
                        rle_prev_v = ret;
                        rle_state = RLE_STATE_SINGLE;
                    }
                }
            }
            else {
                ret = reader.readBits(bpp);
                rle_prev_v = ret;
                rle_state = RLE_STATE_SINGLE;
            }
        }
        else if (rle_state == RLE_STATE_COUNTER) {
            ret = rle_prev_v;
            rle_cnt--;
            if (rle_cnt == 0) {
                ret = reader.readBits(bpp);
                rle_prev_v = ret;
                rle_state = RLE_STATE_SINGLE;
            }
        }

        pixels.append(ret);
    }

    return pixels;
}

void RLEDecompressor::reversePrefilter(QVector<uint8_t> &pixels, int width, int height)
{
    for (int y = 1; y < height; y++) {
        for (int x = 0; x < width; x++) {
            int idx = y * width + x;
            int prevIdx = (y - 1) * width + x;
            pixels[idx] ^= pixels[prevIdx];
        }
    }
}
