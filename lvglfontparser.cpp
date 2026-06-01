#include "lvglfontparser.h"
#include "bitreader.h"
#include "rledecompressor.h"
#include "rlecompressor.h"
#include <QFile>
#include <QFileInfo>
#include <QTextStream>
#include <QRegularExpression>
#include <QDebug>
#include <QDataStream>

LvglFontParser::LvglFontParser()
{
}

bool LvglFontParser::parseFile(const QString &filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        m_error = "Cannot open file: " + filePath;
        return false;
    }

    if (filePath.endsWith(".c", Qt::CaseInsensitive)) {
        QTextStream in(&file);
        QString content = in.readAll();
        return parseCFile(content);
    } else if (filePath.endsWith(".bin", Qt::CaseInsensitive)) {
        QByteArray data = file.readAll();
        return parseBinFile(data);
    } else {
        m_error = "Unsupported file format";
        return false;
    }
}

bool LvglFontParser::parseCFile(const QString &content)
{
    m_font.glyphs.clear();
    m_font.bpp = 4;
    m_font.lineHeight = 16;
    m_font.baseHeight = 16;

    QRegularExpression bitmapArrayRegex(R"(glyph_bitmap\[\]\s*=\s*\{([\s\S]*?)\};)", QRegularExpression::MultilineOption);
    QRegularExpression glyphDescRegex(R"(\{\s*\.bitmap_index\s*=\s*(\d+)\s*,\s*\.adv_w\s*=\s*(\d+)\s*,\s*\.box_w\s*=\s*(\d+)\s*,\s*\.box_h\s*=\s*(\d+)\s*,\s*\.ofs_x\s*=\s*(-?\d+)\s*,\s*\.ofs_y\s*=\s*(-?\d+)\s*\})");
    QRegularExpression cmapRegex(R"(\.range_start\s*=\s*(\d+)\s*,\s*\.range_length\s*=\s*(\d+)\s*,\s*\.glyph_id_start\s*=\s*(\d+))");
    QRegularExpression lineHeightRegex(R"(\.line_height\s*=\s*(\d+))");
    QRegularExpression baseHeightRegex(R"(\.base_line\s*=\s*(\d+))");
    QRegularExpression bppRegex(R"(Bpp:\s*(\d+))");
    QRegularExpression bitmapFormatRegex(R"(\.bitmap_format\s*=\s*(\d+))");

    auto lineHeightMatch = lineHeightRegex.match(content);
    if (lineHeightMatch.hasMatch()) {
        m_font.lineHeight = lineHeightMatch.captured(1).toUInt();
    }

    auto baseHeightMatch = baseHeightRegex.match(content);
    if (baseHeightMatch.hasMatch()) {
        m_font.baseHeight = baseHeightMatch.captured(1).toUInt();
    }

    auto bppMatch = bppRegex.match(content);
    if (bppMatch.hasMatch()) {
        m_font.bpp = bppMatch.captured(1).toUInt();
        qDebug() << "BPP from header:" << m_font.bpp;
    }

    int bitmapFormat = 0;
    auto bitmapFormatMatch = bitmapFormatRegex.match(content);
    if (bitmapFormatMatch.hasMatch()) {
        bitmapFormat = bitmapFormatMatch.captured(1).toInt();
        qDebug() << "Bitmap format:" << bitmapFormat << "(0=raw, 1=compressed)";
    }

    QVector<uint8_t> bitmapData;
    auto bitmapMatch = bitmapArrayRegex.match(content);
    if (bitmapMatch.hasMatch()) {
        QString bitmapStr = bitmapMatch.captured(1);
        QRegularExpression hexRegex(R"(0x([0-9A-Fa-f]{1,2}))");
        auto it = hexRegex.globalMatch(bitmapStr);
        while (it.hasNext()) {
            auto match = it.next();
            bitmapData.append(match.captured(1).toUInt(nullptr, 16));
        }
        qDebug() << "Bitmap data size:" << bitmapData.size();
    } else {
        qDebug() << "Bitmap array not found";
    }

    struct GlyphDesc {
        uint32_t bitmapIndex;
        uint16_t advWidth;
        uint8_t boxW;
        uint8_t boxH;
        int8_t ofsX;
        int8_t ofsY;
    };
    QVector<GlyphDesc> glyphDescs;

    auto glyphIt = glyphDescRegex.globalMatch(content);
    while (glyphIt.hasNext()) {
        auto match = glyphIt.next();
        GlyphDesc desc;
        desc.bitmapIndex = match.captured(1).toUInt();
        desc.advWidth = match.captured(2).toUInt();
        desc.boxW = match.captured(3).toUInt();
        desc.boxH = match.captured(4).toUInt();
        desc.ofsX = match.captured(5).toInt();
        desc.ofsY = match.captured(6).toInt();
        glyphDescs.append(desc);
    }
    qDebug() << "Glyph descriptors found:" << glyphDescs.size();

    auto cmapIt = cmapRegex.globalMatch(content);
    int cmapCount = 0;
    while (cmapIt.hasNext()) {
        auto match = cmapIt.next();
        uint32_t rangeStart = match.captured(1).toUInt();
        uint32_t rangeLength = match.captured(2).toUInt();
        uint32_t glyphIdStart = match.captured(3).toUInt();
        cmapCount++;

        qDebug() << "Cmap range:" << rangeStart << "length:" << rangeLength << "glyph_id_start:" << glyphIdStart;

        for (uint32_t i = 0; i < rangeLength; i++) {
            uint32_t unicode = rangeStart + i;
            uint32_t glyphId = glyphIdStart + i;

            if (glyphId < (uint32_t)glyphDescs.size()) {
                const GlyphDesc &desc = glyphDescs[glyphId];

                LvglGlyph glyph;
                glyph.unicode = unicode;
                glyph.width = desc.boxW;
                glyph.height = desc.boxH;
                glyph.advanceWidth = desc.advWidth;
                glyph.bearingX = desc.ofsX;
                glyph.bearingY = desc.ofsY;
                glyph.bpp = m_font.bpp;
                glyph.use4BitAlignment = true;

                if (bitmapFormat == 1) {
                    int pixelCount = desc.boxW * desc.boxH;

                    if (unicode == 33) {
                        qDebug() << "Decompressing glyph '!' from .c file:";
                        qDebug() << "  Size:" << desc.boxW << "x" << desc.boxH << "=" << pixelCount << "pixels";
                        qDebug() << "  Bitmap index:" << desc.bitmapIndex;
                        qDebug() << "  Available data:" << bitmapData.size() << "bytes";
                    }

                    QByteArray compressedData;
                    for (int i = desc.bitmapIndex; i < bitmapData.size(); i++) {
                        compressedData.append(bitmapData[i]);
                    }
                    QDataStream compStream(compressedData);
                    compStream.setByteOrder(QDataStream::LittleEndian);
                    BitReader bitReader(compStream);

                    QVector<uint8_t> pixels = RLEDecompressor::decompress(bitReader, pixelCount, m_font.bpp);

                    if (unicode == 33) {
                        qDebug() << "  Before prefilter:" << pixels.mid(0, qMin(20, pixels.size()));
                    }

                    RLEDecompressor::reversePrefilter(pixels, desc.boxW, desc.boxH);

                    if (unicode == 33) {
                        qDebug() << "  After prefilter:" << pixels.mid(0, qMin(20, pixels.size()));
                        qDebug() << "  Expected from manual parse: [0, 7, 7, 3, 0, 0, 3, 7, 6, 3, ...]";
                    }
                    int outputBpp = (m_font.bpp == 3) ? 4 : m_font.bpp;
                    int bitsNeeded = pixelCount * outputBpp;
                    int bytesNeeded = (bitsNeeded + 7) / 8;
                    glyph.bitmap.resize(bytesNeeded);
                    glyph.bitmap.fill(0);

                    int bitPos = 0;
                    for (int p = 0; p < pixels.size(); p++) {
                        int byteIdx = bitPos / 8;
                        int bitOffset = bitPos % 8;

                        if (byteIdx < glyph.bitmap.size()) {
                            int shift = 8 - bitOffset - outputBpp;
                            if (shift >= 0) {
                                glyph.bitmap[byteIdx] |= (pixels[p] << shift);
                            } else {
                                glyph.bitmap[byteIdx] |= (pixels[p] >> -shift);
                                if (byteIdx + 1 < glyph.bitmap.size()) {
                                    glyph.bitmap[byteIdx + 1] |= (pixels[p] << (8 + shift));
                                }
                            }
                        }

                        bitPos += outputBpp;
                    }

                    if (unicode == 33) {
                        qDebug() << "  Decompressed" << pixels.size() << "pixels";
                        qDebug() << "  First 10:" << pixels.mid(0, qMin(10, pixels.size()));
                        qDebug() << "  Packed bitmap size:" << glyph.bitmap.size() << "bytes";
                        qDebug() << "  First 8 bytes:";
                        for (int i = 0; i < qMin(8, glyph.bitmap.size()); i++) {
                            qDebug() << "    [" << i << "] = 0x" << QString::number((uint8_t)glyph.bitmap[i], 16);
                        }
                    }
                } else {
                    uint32_t bitmapSize = (desc.boxW * desc.boxH * m_font.bpp + 7) / 8;
                    if (desc.bitmapIndex + bitmapSize <= (uint32_t)bitmapData.size()) {
                        glyph.bitmap = bitmapData.mid(desc.bitmapIndex, bitmapSize);
                    } else {
                        qDebug() << "Warning: bitmap out of range for unicode" << unicode
                                 << "index:" << desc.bitmapIndex << "size:" << bitmapSize
                                 << "available:" << bitmapData.size();
                    }
                }

                if (glyph.width > 0 && glyph.height > 0) {
                    m_font.glyphs[unicode] = glyph;
                }
            }
        }
    }
    qDebug() << "Cmap entries found:" << cmapCount;
    qDebug() << "Total glyphs parsed:" << m_font.glyphs.size();

    if (m_font.glyphs.isEmpty()) {
        m_error = "No glyphs found in file";
        return false;
    }

    return true;
}

bool LvglFontParser::parseBinFile(const QByteArray &data)
{
    if (data.size() < 64) {
        m_error = "Binary file too small";
        return false;
    }

    m_font.glyphs.clear();
    QDataStream stream(data);
    stream.setByteOrder(QDataStream::LittleEndian);

    uint32_t headSize;
    stream >> headSize;
    
    char headLabel[5] = {0};
    stream.readRawData(headLabel, 4);
    
    if (QString(headLabel) != "head") {
        m_error = "Invalid binary format: 'head' table not found";
        return false;
    }

    uint32_t version;
    uint16_t tables_count, font_size, ascent;
    int16_t descent;
    uint16_t typo_ascent;
    int16_t typo_descent;
    uint16_t typo_line_gap;
    int16_t min_y, max_y;
    uint16_t default_advance_width, kerning_scale;
    uint8_t index_to_loc_format, glyph_id_format, advance_width_format;
    uint8_t bits_per_pixel, xy_bits, wh_bits, advance_width_bits;
    uint8_t compression_id, subpixels_mode, padding;
    int16_t underline_position;
    uint16_t underline_thickness;
    
    stream >> version >> tables_count >> font_size >> ascent >> descent;
    stream >> typo_ascent >> typo_descent >> typo_line_gap;
    stream >> min_y >> max_y >> default_advance_width >> kerning_scale;
    stream >> index_to_loc_format >> glyph_id_format >> advance_width_format;
    stream >> bits_per_pixel >> xy_bits >> wh_bits >> advance_width_bits;
    stream >> compression_id >> subpixels_mode >> padding;
    stream >> underline_position >> underline_thickness;
    
    m_font.bpp = bits_per_pixel;
    m_font.lineHeight = font_size;
    m_font.baseHeight = ascent;
    m_font.ascent = ascent;
    m_font.descent = descent;
    
    qDebug() << "Bin header: bpp=" << bits_per_pixel << "xy_bits=" << xy_bits
             << "wh_bits=" << wh_bits << "adv_bits=" << advance_width_bits
             << "compression=" << compression_id;

    stream.device()->seek((headSize + 3) & ~3);
    uint32_t cmapSize;
    stream >> cmapSize;
    
    char cmapLabel[5] = {0};
    stream.readRawData(cmapLabel, 4);
    
    if (QString(cmapLabel) != "cmap") {
        m_error = "Invalid binary format: 'cmap' table not found";
        return false;
    }
    
    uint32_t cmap_count;
    stream >> cmap_count;

    uint32_t data_offset, range_start;
    uint16_t range_length, glyph_id_start, entries_count;
    uint8_t format_type, cmap_padding;
    
    stream >> data_offset >> range_start >> range_length >> glyph_id_start;
    stream >> entries_count >> format_type >> cmap_padding;
    
    qDebug() << "Cmap: range=" << range_start << "-" << (range_start + range_length - 1)
             << "glyph_id_start=" << glyph_id_start;

    qint64 cmapStart = (headSize + 3) & ~3;
    stream.device()->seek((cmapStart + cmapSize + 3) & ~3);

    uint32_t locaSize;
    stream >> locaSize;
    
    char locaLabel[5] = {0};
    stream.readRawData(locaLabel, 4);
    
    if (QString(locaLabel) != "loca") {
        m_error = "Invalid binary format: 'loca' table not found";
        return false;
    }
    
    uint32_t loca_count;
    stream >> loca_count;
    
    QVector<uint32_t> offsets;
    for (uint32_t i = 0; i < loca_count; i++) {
        if (index_to_loc_format == 0) {
            uint16_t offset;
            stream >> offset;
            offsets.append(offset);
        } else {
            uint32_t offset;
            stream >> offset;
            offsets.append(offset);
        }
    }
    
    qDebug() << "Loca: count=" << loca_count;

    qint64 locaStart = (cmapStart + cmapSize + 3) & ~3;
    stream.device()->seek((locaStart + locaSize + 3) & ~3);

    uint32_t glyfSize;
    stream >> glyfSize;
    
    char glyfLabel[5] = {0};
    stream.readRawData(glyfLabel, 4);
    
    if (QString(glyfLabel) != "glyf") {
        m_error = "Invalid binary format: 'glyf' table not found";
        return false;
    }

    qint64 glyfTableStart = (locaStart + locaSize + 3) & ~3;
    qint64 glyfDataStart = glyfTableStart;  // Offsets include the 8-byte header
    bool monospaced = (default_advance_width != 0);

    qDebug() << "Glyf: table at" << glyfTableStart << "parsing" << range_length << "glyphs";

    for (uint16_t i = 0; i < range_length && (glyph_id_start + i) < loca_count - 1; i++) {
        uint32_t unicode = range_start + i;
        uint32_t glyphId = glyph_id_start + i;
        
        uint32_t glyphOffset = offsets[glyphId];
        uint32_t nextOffset = offsets[glyphId + 1];
        
        if (glyphOffset == nextOffset) continue;

        stream.device()->seek(glyfDataStart + glyphOffset);

        if (unicode == 33) {
            qint64 savedPos = stream.device()->pos();
            int glyphSize = nextOffset - glyphOffset;
            QByteArray rawGlyphData(glyphSize, 0);
            stream.readRawData(rawGlyphData.data(), glyphSize);
            qDebug() << "Original glyph 33 raw data (" << glyphSize << "bytes):" << rawGlyphData.toHex();
            stream.device()->seek(savedPos);
        }

        BitReader bitReader(stream);

        LvglGlyph glyph;
        glyph.unicode = unicode;
        glyph.bpp = bits_per_pixel;
        glyph.use4BitAlignment = false;

        if (monospaced) {
            glyph.advanceWidth = default_advance_width;
        } else if (advance_width_bits > 0) {
            int32_t adv = bitReader.readBitsSigned(advance_width_bits);
            glyph.advanceWidth = (advance_width_format == 0) ? adv * 16 : adv;
        }

        glyph.bearingX = bitReader.readBitsSigned(xy_bits);
        glyph.bearingY = bitReader.readBitsSigned(xy_bits);
        glyph.width = bitReader.readBits(wh_bits);
        glyph.height = bitReader.readBits(wh_bits);

        if (unicode == 33) {
            qDebug() << "  After reading header: advW=" << glyph.advanceWidth
                     << "bearX=" << glyph.bearingX << "bearY=" << glyph.bearingY
                     << "w=" << glyph.width << "h=" << glyph.height;
        }
        
        if (glyph.width == 0 || glyph.height == 0) continue;

        // Read bitmap
        int headerBits = (monospaced ? 0 : advance_width_bits) + 2 * xy_bits + 2 * wh_bits;
        int totalBytes = nextOffset - glyphOffset;
        int bitmapBytes = totalBytes - (headerBits + 7) / 8;

        if (i < 3) {
            qDebug() << "  Glyph" << i << "unicode=" << unicode
                     << "size=" << glyph.width << "x" << glyph.height
                     << "headerBits=" << headerBits << "totalBytes=" << totalBytes
                     << "bitmapBytes=" << bitmapBytes << "compression=" << compression_id;
        }

        if (bitmapBytes > 0) {
            if (compression_id == 0) {
                glyph.bitmap.resize(bitmapBytes);
                if (headerBits % 8 == 0) {
                    for (int j = 0; j < bitmapBytes; j++) {
                        uint8_t byte;
                        stream >> byte;
                        glyph.bitmap[j] = byte;
                    }
                } else {
                    for (int j = 0; j < bitmapBytes; j++) {
                        glyph.bitmap[j] = bitReader.readBits(8);
                    }
                    if (unicode == 33) {
                        qDebug() << "  Read bitmap via BitReader:" << QByteArray(reinterpret_cast<const char*>(glyph.bitmap.constData()), glyph.bitmap.size()).toHex();
                    }
                }
            } else {
                int pixelCount = glyph.width * glyph.height;
                QVector<uint8_t> pixels = RLEDecompressor::decompress(bitReader, pixelCount, bits_per_pixel);

                RLEDecompressor::reversePrefilter(pixels, glyph.width, glyph.height);

                if (i < 3) {
                    qDebug() << "    Decompressed" << pixels.size() << "pixels (expected" << pixelCount << ")";
                    if (pixels.size() > 0) {
                        qDebug() << "    First 10 pixels:" << pixels.mid(0, qMin(10, pixels.size()));
                    }
                }

                int bitsNeeded = pixelCount * bits_per_pixel;
                int bytesNeeded = (bitsNeeded + 7) / 8;
                glyph.bitmap.resize(bytesNeeded);
                glyph.bitmap.fill(0);

                int bitPos = 0;
                for (int p = 0; p < pixels.size(); p++) {
                    int byteIdx = bitPos / 8;
                    int bitOffset = bitPos % 8;

                    if (byteIdx < glyph.bitmap.size()) {
                        int shift = 8 - bitOffset - bits_per_pixel;
                        if (shift >= 0) {
                            glyph.bitmap[byteIdx] |= (pixels[p] << shift);
                        } else {
                            glyph.bitmap[byteIdx] |= (pixels[p] >> -shift);
                            if (byteIdx + 1 < glyph.bitmap.size()) {
                                glyph.bitmap[byteIdx + 1] |= (pixels[p] << (8 + shift));
                            }
                        }
                    }

                    bitPos += bits_per_pixel;
                }
            }
        }
        
        m_font.glyphs[unicode] = glyph;
    }
    
    qDebug() << "Binary font parsed:" << m_font.glyphs.size() << "glyphs";
    
    if (m_font.glyphs.isEmpty()) {
        m_error = "No glyphs found";
        return false;
    }
    
    return true;
}

bool LvglFontParser::saveToFile(const QString &filePath, const LvglFont &font)
{
    if (filePath.endsWith(".bin", Qt::CaseInsensitive)) {
        return saveToBinFile(filePath, font);
    } else {
        return saveToCFile(filePath, font);
    }
}

bool LvglFontParser::saveToBinFile(const QString &filePath, const LvglFont &font)
{
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        m_error = "Cannot open file for writing: " + filePath;
        return false;
    }

    QDataStream stream(&file);
    stream.setByteOrder(QDataStream::LittleEndian);

    QVector<uint32_t> unicodeList = font.glyphs.keys().toVector();
    std::sort(unicodeList.begin(), unicodeList.end());

    QVector<QByteArray> glyphData;
    QVector<uint32_t> glyphOffsets;

    const uint32_t GLYF_HEAD_LENGTH = 8;
    uint32_t currentOffset = GLYF_HEAD_LENGTH;

    glyphOffsets.append(currentOffset);

    for (uint32_t unicode : unicodeList) {
        const LvglGlyph &glyph = font.glyphs[unicode];
        glyphOffsets.append(currentOffset);

        if (glyph.width == 0 || glyph.height == 0) {
            glyphData.append(QByteArray());
            continue;
        }

        class BitWriter {
        public:
            void writeBits(uint32_t value, int numBits) {
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
            QByteArray getData() {
                if (m_bitPos > 0) {
                    m_currentByte <<= (8 - m_bitPos);
                    m_data.append(m_currentByte);
                }
                return m_data;
            }
        private:
            QByteArray m_data;
            int m_bitPos = 0;
            uint8_t m_currentByte = 0;
        };

        BitWriter writer;

        writer.writeBits(glyph.advanceWidth, 10);
        writer.writeBits(glyph.bearingX & 0x1F, 5);
        writer.writeBits(glyph.bearingY & 0x1F, 5);
        writer.writeBits(glyph.width, 5);
        writer.writeBits(glyph.height, 5);

        if (font.bpp == 3) {
            int pixelCount = glyph.width * glyph.height;
            QVector<uint8_t> pixels(pixelCount);
            int maxValue = 7;

            int readBpp = glyph.use4BitAlignment ? 4 : 3;

            for (int i = 0; i < pixelCount; i++) {
                int bitPos = i * readBpp;
                int byteIndex = bitPos / 8;
                int bitOffset = bitPos % 8;
                int bitsInFirstByte = 8 - bitOffset;

                if (byteIndex < glyph.bitmap.size()) {
                    if (bitsInFirstByte >= readBpp) {
                        int shift = bitsInFirstByte - readBpp;
                        pixels[i] = (glyph.bitmap[byteIndex] >> shift) & maxValue;
                    } else {
                        int bitsFromSecondByte = readBpp - bitsInFirstByte;
                        int firstPart = (glyph.bitmap[byteIndex] & ((1 << bitsInFirstByte) - 1)) << bitsFromSecondByte;
                        int secondPart = 0;
                        if (byteIndex + 1 < glyph.bitmap.size()) {
                            secondPart = glyph.bitmap[byteIndex + 1] >> (8 - bitsFromSecondByte);
                        }
                        pixels[i] = (firstPart | secondPart) & maxValue;
                    }
                }
            }

            if (unicode == 33 || unicode == 65) {
                qDebug() << "  3bpp save unicode" << unicode << ": pixelCount=" << pixelCount << "readBpp=" << readBpp << "use4BitAlignment=" << glyph.use4BitAlignment;
                qDebug() << "  First 10 pixels before prefilter:" << pixels.mid(0, qMin(10, pixels.size()));
            }

            RLECompressor::applyPrefilter(pixels, glyph.width, glyph.height);

            if (unicode == 33 || unicode == 65) {
                qDebug() << "  First 10 pixels after prefilter:" << pixels.mid(0, qMin(10, pixels.size()));
            }

            QVector<uint8_t> compressed = RLECompressor::compress(pixels, font.bpp);

            if (unicode == 33 || unicode == 65) {
                qDebug() << "  Compressed size:" << compressed.size() << "bytes";
                if (compressed.size() <= 30) {
                    qDebug() << "  Compressed data:" << QByteArray(reinterpret_cast<const char*>(compressed.constData()), compressed.size()).toHex();
                }
            }

            for (uint8_t byte : compressed) {
                writer.writeBits(byte, 8);
            }

            if (unicode == 33) {
                qDebug() << "  After writing compressed data to BitWriter";
            }
        } else {
            for (uint8_t byte : glyph.bitmap) {
                writer.writeBits(byte, 8);
            }
        }

        QByteArray glyphBytes = writer.getData();

        if (unicode == 33 || unicode == 65) {  // '!' or 'A'
            qDebug() << "Saving glyph unicode" << unicode << ":";
            qDebug() << "  advanceWidth:" << glyph.advanceWidth << "bearingX:" << glyph.bearingX << "bearingY:" << glyph.bearingY;
            qDebug() << "  width:" << glyph.width << "height:" << glyph.height;
            qDebug() << "  Original bitmap size:" << glyph.bitmap.size();
            if (glyph.bitmap.size() > 0 && glyph.bitmap.size() <= 30) {
                qDebug() << "  Original bitmap data:" << QByteArray(reinterpret_cast<const char*>(glyph.bitmap.constData()), glyph.bitmap.size()).toHex();
            }
            qDebug() << "  Total glyph size:" << glyphBytes.size() << "bytes";
            if (glyphBytes.size() <= 50) {
                qDebug() << "  Complete glyph data:" << glyphBytes.toHex();
            }
        }

        glyphData.append(glyphBytes);
        currentOffset += glyphBytes.size();
    }

    glyphOffsets.append(currentOffset);

    uint32_t headSize = 48;
    stream << headSize;
    stream.writeRawData("head", 4);
    stream << (uint32_t)1;
    stream << (uint16_t)4;
    stream << (uint16_t)font.lineHeight;
    stream << (uint16_t)(font.ascent > 0 ? font.ascent : font.baseHeight);
    stream << (int16_t)(font.descent != 0 ? font.descent : 0);
    stream << (uint16_t)font.baseHeight;
    stream << (int16_t)0;
    stream << (uint16_t)0;
    stream << (int16_t)0;
    stream << (int16_t)font.lineHeight;
    stream << (uint16_t)0;
    stream << (uint16_t)16;
    stream << (uint8_t)0;
    stream << (uint8_t)0;
    stream << (uint8_t)0;
    stream << (uint8_t)font.bpp;
    stream << (uint8_t)5;
    stream << (uint8_t)5;
    stream << (uint8_t)10;
    stream << (uint8_t)(font.bpp == 3 ? 1 : 0);
    stream << (uint8_t)0;
    stream << (uint8_t)0;
    stream << (int16_t)0;
    stream << (uint16_t)0;

    while (file.pos() % 4 != 0) {
        stream << (uint8_t)0;
    }

    qint64 cmapStart = file.pos();
    uint32_t cmapSize = 4 + 4 + 4 + 16;
    stream << cmapSize;
    stream.writeRawData("cmap", 4);
    stream << (uint32_t)1;
    stream << (uint32_t)0;
    stream << (uint32_t)unicodeList.first();
    stream << (uint16_t)unicodeList.size();
    stream << (uint16_t)1;
    stream << (uint16_t)0;
    stream << (uint8_t)0;
    stream << (uint8_t)0;

    while (file.pos() % 4 != 0) {
        stream << (uint8_t)0;
    }

    qint64 locaStart = file.pos();
    uint32_t locaSize = 4 + 4 + 4 + glyphOffsets.size() * 2;
    stream << locaSize;
    stream.writeRawData("loca", 4);
    stream << (uint32_t)glyphOffsets.size();
    for (uint32_t offset : glyphOffsets) {
        stream << (uint16_t)offset;
    }

    while (file.pos() % 4 != 0) {
        stream << (uint8_t)0;
    }

    qint64 glyfStart = file.pos();
    uint32_t glyfSize = 4 + 4 + currentOffset;
    stream << glyfSize;
    stream.writeRawData("glyf", 4);
    for (const QByteArray &data : glyphData) {
        stream.writeRawData(data.constData(), data.size());
    }

    file.close();
    return true;
}

bool LvglFontParser::saveToCFile(const QString &filePath, const LvglFont &font)
{
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        m_error = "Cannot open file for writing: " + filePath;
        return false;
    }

    QTextStream out(&file);
    QString fontName = QFileInfo(filePath).baseName();

    out << "/*******************************************************************************\n";
    out << " * Generated by LVGL Font Viewer\n";
    out << " * Size: " << font.lineHeight << " px\n";
    out << " * Bpp: " << font.bpp << "\n";
    out << " ******************************************************************************/\n\n";

    out << "#ifdef LV_LVGL_H_INCLUDE_SIMPLE\n";
    out << "    #include \"lvgl.h\"\n";
    out << "#else\n";
    out << "    #include \"lvgl/lvgl.h\"\n";
    out << "#endif\n\n";

    out << "#ifndef " << fontName.toUpper() << "\n";
    out << "#define " << fontName.toUpper() << " 1\n";
    out << "#endif\n\n";

    out << "#if " << fontName.toUpper() << "\n\n";

    QVector<uint8_t> bitmapData;
    QVector<uint32_t> unicodeList = font.glyphs.keys().toVector();
    std::sort(unicodeList.begin(), unicodeList.end());

    out << "/*Store the image of the glyphs*/\n";
    out << "static LV_ATTRIBUTE_LARGE_CONST const uint8_t glyph_bitmap[] = {\n";

    for (uint32_t unicode : unicodeList) {
        const LvglGlyph &glyph = font.glyphs[unicode];
        out << "    /* U+" << QString::number(unicode, 16).toUpper().rightJustified(4, '0');
        if (unicode >= 32 && unicode < 127) {
            out << " \"" << QChar(unicode) << "\"";
        }
        out << " */\n";

        if (glyph.bitmap.isEmpty()) {
            out << "\n";
        } else {
            QVector<uint8_t> outputBitmap;

            // For 3bpp, we need to compress the data
            if (glyph.bpp == 3) {
                // Extract pixels from 4-bit aligned data
                int pixelCount = glyph.width * glyph.height;
                QVector<uint8_t> pixels(pixelCount);
                int maxValue = 7;

                for (int i = 0; i < pixelCount; i++) {
                    int bitPos = i * 4;  // 4-bit aligned
                    int byteIndex = bitPos / 8;
                    int bitOffset = bitPos % 8;
                    int bitsInFirstByte = 8 - bitOffset;

                    if (byteIndex < glyph.bitmap.size()) {
                        if (bitsInFirstByte >= 4) {
                            int shift = bitsInFirstByte - 4;
                            pixels[i] = (glyph.bitmap[byteIndex] >> shift) & maxValue;
                        } else {
                            int bitsFromSecondByte = 4 - bitsInFirstByte;
                            int firstPart = (glyph.bitmap[byteIndex] & ((1 << bitsInFirstByte) - 1)) << bitsFromSecondByte;
                            int secondPart = 0;
                            if (byteIndex + 1 < glyph.bitmap.size()) {
                                secondPart = glyph.bitmap[byteIndex + 1] >> (8 - bitsFromSecondByte);
                            }
                            pixels[i] = (firstPart | secondPart) & maxValue;
                        }
                    }
                }

                if (unicode == 0x21) {  // '!'
                    qDebug() << "Saving '!': compressing";
                    qDebug() << "  Size:" << glyph.width << "x" << glyph.height << "=" << pixelCount << "pixels";
                    qDebug() << "  Extracted pixels (first 10):" << pixels.mid(0, qMin(10, pixels.size()));
                }

                // Apply prefilter (XOR with previous line)
                RLECompressor::applyPrefilter(pixels, glyph.width, glyph.height);

                if (unicode == 0x21) {
                    qDebug() << "  After prefilter (first 10):" << pixels.mid(0, qMin(10, pixels.size()));
                }

                outputBitmap = RLECompressor::compress(pixels, glyph.bpp);

                if (unicode == 0x21) {
                    qDebug() << "  Compressed size:" << outputBitmap.size() << "bytes";
                    qDebug() << "  First 8 bytes:" << outputBitmap.mid(0, qMin(8, outputBitmap.size()));
                }
            } else {
                outputBitmap = glyph.bitmap;
            }

            out << "    ";
            for (int i = 0; i < outputBitmap.size(); i++) {
                out << "0x" << QString::number(outputBitmap[i], 16).rightJustified(2, '0');
                if (i < outputBitmap.size() - 1) {
                    out << ", ";
                    if ((i + 1) % 12 == 0) {
                        out << "\n    ";
                    }
                }
            }
            out << ",\n\n";
            bitmapData.append(outputBitmap);
        }
    }

    out << "};\n\n";

    QMap<uint32_t, uint32_t> bitmapIndices;
    uint32_t currentIndex = 0;

    for (uint32_t unicode : unicodeList) {
        const LvglGlyph &glyph = font.glyphs[unicode];
        bitmapIndices[unicode] = currentIndex;

        if (!glyph.bitmap.isEmpty()) {
            if (glyph.bpp == 3) {
                int pixelCount = glyph.width * glyph.height;
                QVector<uint8_t> pixels(pixelCount);
                int maxValue = 7;

                for (int i = 0; i < pixelCount; i++) {
                    int bitPos = i * 4;
                    int byteIndex = bitPos / 8;
                    int bitOffset = bitPos % 8;
                    int bitsInFirstByte = 8 - bitOffset;

                    if (byteIndex < glyph.bitmap.size()) {
                        if (bitsInFirstByte >= 4) {
                            int shift = bitsInFirstByte - 4;
                            pixels[i] = (glyph.bitmap[byteIndex] >> shift) & maxValue;
                        } else {
                            int bitsFromSecondByte = 4 - bitsInFirstByte;
                            int firstPart = (glyph.bitmap[byteIndex] & ((1 << bitsInFirstByte) - 1)) << bitsFromSecondByte;
                            int secondPart = 0;
                            if (byteIndex + 1 < glyph.bitmap.size()) {
                                secondPart = glyph.bitmap[byteIndex + 1] >> (8 - bitsFromSecondByte);
                            }
                            pixels[i] = (firstPart | secondPart) & maxValue;
                        }
                    }
                }

                RLECompressor::applyPrefilter(pixels, glyph.width, glyph.height);
                QVector<uint8_t> compressed = RLECompressor::compress(pixels, glyph.bpp);
                currentIndex += compressed.size();
            } else {
                currentIndex += glyph.bitmap.size();
            }
        }
    }

    out << "static const lv_font_fmt_txt_glyph_dsc_t glyph_dsc[] = {\n";
    out << "    {.bitmap_index = 0, .adv_w = 0, .box_w = 0, .box_h = 0, .ofs_x = 0, .ofs_y = 0} /* id = 0 reserved */,\n";

    for (uint32_t unicode : unicodeList) {
        const LvglGlyph &glyph = font.glyphs[unicode];
        out << "    {.bitmap_index = " << bitmapIndices[unicode]
            << ", .adv_w = " << glyph.advanceWidth
            << ", .box_w = " << glyph.width
            << ", .box_h = " << glyph.height
            << ", .ofs_x = " << glyph.bearingX
            << ", .ofs_y = " << glyph.bearingY << "}";

        if (unicode != unicodeList.last()) {
            out << ",";
        }
        out << "\n";
    }

    out << "};\n\n";

    out << "static const lv_font_fmt_txt_cmap_t cmaps[] = {\n";
    out << "    {\n";
    out << "        .range_start = " << unicodeList.first() << ", .range_length = " << unicodeList.size()
        << ", .glyph_id_start = 1,\n";
    out << "        .unicode_list = NULL, .glyph_id_ofs_list = NULL, .list_length = 0, .type = LV_FONT_FMT_TXT_CMAP_FORMAT0_TINY\n";
    out << "    }\n";
    out << "};\n\n";

    out << "lv_font_fmt_txt_dsc_t font_dsc = {\n";
    out << "    .glyph_bitmap = glyph_bitmap,\n";
    out << "    .glyph_dsc = glyph_dsc,\n";
    out << "    .cmaps = cmaps,\n";
    out << "    .kern_dsc = NULL,\n";
    out << "    .kern_scale = 0,\n";
    out << "    .cmap_num = 1,\n";
    out << "    .bpp = " << font.bpp << ",\n";
    out << "    .kern_classes = 0,\n";
    out << "    .bitmap_format = " << (font.bpp == 3 ? 1 : 0) << "\n";
    out << "};\n\n";

    out << "lv_font_t " << fontName << " = {\n";
    out << "    .get_glyph_dsc = lv_font_get_glyph_dsc_fmt_txt,\n";
    out << "    .get_glyph_bitmap = lv_font_get_bitmap_fmt_txt,\n";
    out << "    .line_height = " << font.lineHeight << ",\n";
    out << "    .base_line = " << font.baseHeight << ",\n";
    out << "    .dsc = &font_dsc\n";
    out << "};\n\n";

    out << "#endif\n";

    file.close();
    return true;
}
