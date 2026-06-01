#include "bitreader.h"
#include <QDebug>

BitReader::BitReader(QDataStream &stream)
    : m_stream(stream)
    , m_bitPos(-1)
    , m_byteValue(0)
{
}

uint32_t BitReader::readBits(int nBits)
{
    uint32_t value = 0;
    static bool debug = false;

    while (nBits > 0) {
        if (m_bitPos < 0) {
            m_bitPos = 7;
            m_stream >> m_byteValue;

            if (debug) {
                qDebug() << "    BitReader: read new byte 0x" << QString::number(m_byteValue, 16);
            }

            if (m_stream.status() != QDataStream::Ok) {
                return 0;
            }
        }

        int8_t bit = (m_byteValue >> m_bitPos) & 1;
        value = (value << 1) | bit;
        m_bitPos--;
        nBits--;
    }

    return value;
}

int32_t BitReader::readBitsSigned(int nBits)
{
    uint32_t value = readBits(nBits);

    if (value & (1 << (nBits - 1))) {
        value |= ~0u << nBits;
    }

    return static_cast<int32_t>(value);
}

bool BitReader::atEnd() const
{
    return m_stream.atEnd();
}
