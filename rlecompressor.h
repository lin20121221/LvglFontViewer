#ifndef RLECOMPRESSOR_H
#define RLECOMPRESSOR_H

#include <QVector>
#include <cstdint>

class RLECompressor
{
public:
    static QVector<uint8_t> compress(const QVector<uint8_t> &pixels, int bpp);
    static void applyPrefilter(QVector<uint8_t> &pixels, int width, int height);

private:
    static constexpr int RLE_SKIP_COUNT = 1;
    static constexpr int RLE_BIT_COLLAPSED_COUNT = 10;
    static constexpr int RLE_COUNTER_BITS = 6;
};

#endif
