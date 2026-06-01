#ifndef LVGLFONTPARSER_H
#define LVGLFONTPARSER_H

#include <QString>
#include <QVector>
#include <QMap>
#include <cstdint>

struct LvglGlyph {
    uint32_t unicode;
    uint16_t width;
    uint16_t height;
    int16_t advanceWidth;
    int16_t bearingX;
    int16_t bearingY;
    QVector<uint8_t> bitmap;
    uint8_t bpp;
    bool use4BitAlignment;
};

struct LvglFont {
    QString name;
    uint16_t lineHeight;
    uint16_t baseHeight;
    uint8_t bpp;
    QMap<uint32_t, LvglGlyph> glyphs;

    uint16_t ascent;
    int16_t descent;
};

class LvglFontParser
{
public:
    LvglFontParser();

    bool parseFile(const QString &filePath);
    bool saveToFile(const QString &filePath, const LvglFont &font);
    const LvglFont& getFont() const { return m_font; }
    QString getError() const { return m_error; }

private:
    bool parseCFile(const QString &content);
    bool parseBinFile(const QByteArray &data);
    bool saveToCFile(const QString &filePath, const LvglFont &font);
    bool saveToBinFile(const QString &filePath, const LvglFont &font);

    LvglFont m_font;
    QString m_error;
};

#endif
