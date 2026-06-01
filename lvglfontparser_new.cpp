#include "lvglfontparser.h"
#include "bitreader.h"
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

                uint32_t bitmapSize = (desc.boxW * desc.boxH * m_font.bpp + 7) / 8;
                if (desc.bitmapIndex + bitmapSize <= (uint32_t)bitmapData.size()) {
                    glyph.bitmap = bitmapData.mid(desc.bitmapIndex, bitmapSize);
                } else {
                    qDebug() << "Warning: bitmap out of range for unicode" << unicode
                             << "index:" << desc.bitmapIndex << "size:" << bitmapSize
                             << "available:" << bitmapData.size();
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


bool LvglFontParser::saveToFile(const QString &filePath, const LvglFont &font)
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
            out << "    ";
            for (int i = 0; i < glyph.bitmap.size(); i++) {
                out << "0x" << QString::number(glyph.bitmap[i], 16).rightJustified(2, '0');
                if (i < glyph.bitmap.size() - 1) {
                    out << ", ";
                    if ((i + 1) % 12 == 0) {
                        out << "\n    ";
                    }
                }
            }
            out << ",\n\n";
        }
        bitmapData.append(glyph.bitmap);
    }

    out << "};\n\n";

    out << "static const lv_font_fmt_txt_glyph_dsc_t glyph_dsc[] = {\n";
    out << "    {.bitmap_index = 0, .adv_w = 0, .box_w = 0, .box_h = 0, .ofs_x = 0, .ofs_y = 0} /* id = 0 reserved */,\n";

    uint32_t bitmapIndex = 0;
    for (uint32_t unicode : unicodeList) {
        const LvglGlyph &glyph = font.glyphs[unicode];
        out << "    {.bitmap_index = " << bitmapIndex
            << ", .adv_w = " << glyph.advanceWidth
            << ", .box_w = " << glyph.width
            << ", .box_h = " << glyph.height
            << ", .ofs_x = " << glyph.bearingX
            << ", .ofs_y = " << glyph.bearingY << "}";

        if (unicode != unicodeList.last()) {
            out << ",";
        }
        out << "\n";

        bitmapIndex += glyph.bitmap.size();
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
    out << "    .bitmap_format = 0\n";
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
