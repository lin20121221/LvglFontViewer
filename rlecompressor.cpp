#include "rlecompressor.h"
#include "bitreader.h"
#include <QDataStream>

class BitWriter {
public:
    BitWriter() : m_bitPos(0), m_currentByte(0) {}

    void writeBits(uint8_t value, int numBits) {
        for (int i = numBits - 1; i >= 0; i--) {
            int bit = (value >> i) & 1;
            m_currentByte = (m_currentByte << 1) | bit;
            m_bitPos++;

            if (m_bitPos == 8) {
                m_data.append(m_currentByte);
                m_currentByte = 0;
                m_bitPos = 0;
            }
        }
    }

    QVector<uint8_t> getData() {
        if (m_bitPos > 0) {
            m_currentByte <<= (8 - m_bitPos);
            m_data.append(m_currentByte);
        }
        return m_data;
    }

private:
    QVector<uint8_t> m_data;
    int m_bitPos;
    uint8_t m_currentByte;
};

void RLECompressor::applyPrefilter(QVector<uint8_t> &pixels, int width, int height)
{
    for (int y = height - 1; y >= 1; y--) {
        for (int x = 0; x < width; x++) {
            int idx = y * width + x;
            int prevIdx = (y - 1) * width + x;
            pixels[idx] ^= pixels[prevIdx];
        }
    }
}

QVector<uint8_t> RLECompressor::compress(const QVector<uint8_t> &pixels, int bpp)
{
    BitWriter writer;

    int offset = 0;
    while (offset < pixels.size()) {
        uint8_t p = pixels[offset];

        int same = 1;
        while (offset + same < pixels.size() &&
               pixels[offset + same] == p &&
               same < (1 << RLE_COUNTER_BITS) - 1 + RLE_BIT_COLLAPSED_COUNT + RLE_SKIP_COUNT + 1) {
            same++;
        }

        offset += same;

        if (same <= RLE_SKIP_COUNT) {
            for (int i = 0; i < same; i++) {
                writer.writeBits(p, bpp);
            }
            continue;
        }

        for (int i = 0; i < RLE_SKIP_COUNT; i++) {
            writer.writeBits(p, bpp);
        }

        same -= RLE_SKIP_COUNT;

        if (same <= RLE_BIT_COLLAPSED_COUNT) {
            writer.writeBits(p, bpp);
            for (int i = 0; i < same; i++) {
                if (i < same - 1) {
                    writer.writeBits(1, 1);
                } else {
                    writer.writeBits(0, 1);
                }
            }
            continue;
        }

        same -= RLE_BIT_COLLAPSED_COUNT + 1;

        writer.writeBits(p, bpp);

        for (int i = 0; i < RLE_BIT_COLLAPSED_COUNT + 1; i++) {
            writer.writeBits(1, 1);
        }
        writer.writeBits(same, RLE_COUNTER_BITS);
    }

    return writer.getData();
}
