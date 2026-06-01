#ifndef BITREADER_H
#define BITREADER_H

#include <QDataStream>
#include <cstdint>

class BitReader
{
public:
    explicit BitReader(QDataStream &stream);

    uint32_t readBits(int nBits);
    int32_t readBitsSigned(int nBits);
    bool atEnd() const;

private:
    QDataStream &m_stream;
    int8_t m_bitPos;
    uint8_t m_byteValue;
};

#endif
