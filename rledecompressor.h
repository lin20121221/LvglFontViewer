#ifndef RLEDECOMPRESSOR_H
#define RLEDECOMPRESSOR_H

#include "bitreader.h"
#include <QVector>
#include <cstdint>

class RLEDecompressor
{
public:
    static QVector<uint8_t> decompress(BitReader &reader, int pixelCount, int bpp);
    static void reversePrefilter(QVector<uint8_t> &pixels, int width, int height);

private:
    static constexpr int RLE_SKIP_COUNT = 1;
    static constexpr int RLE_BIT_COLLAPSED_COUNT = 10;
    static constexpr int RLE_COUNTER_BITS = 6;
};

#endif
