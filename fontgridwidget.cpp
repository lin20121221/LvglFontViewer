#include "fontgridwidget.h"
#include <QPainter>
#include <QMouseEvent>
#include <QtMath>

FontGridWidget::FontGridWidget(QWidget *parent)
    : QWidget(parent)
    , m_cellSize(48)
    , m_columns(10)
    , m_selectedIndex(-1)
{
    setMinimumSize(400, 300);
}

void FontGridWidget::setFont(const LvglFont &font)
{
    m_font = font;
    m_unicodeList = m_font.glyphs.keys().toVector();
    std::sort(m_unicodeList.begin(), m_unicodeList.end());
    m_selectedIndex = -1;
    calculateLayout();
    update();
}

void FontGridWidget::clear()
{
    m_font.glyphs.clear();
    m_unicodeList.clear();
    m_selectedIndex = -1;
    update();
}

void FontGridWidget::calculateLayout()
{
    if (width() > 0) {
        m_columns = qMax(1, (width() - 20) / m_cellSize);
        int rows = (m_unicodeList.size() + m_columns - 1) / m_columns;
        setMinimumHeight(rows * m_cellSize + 20);
    }
}

void FontGridWidget::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    calculateLayout();
}

void FontGridWidget::paintEvent(QPaintEvent *event)
{
    QPainter painter(this);
    painter.fillRect(rect(), Qt::white);

    if (m_unicodeList.isEmpty()) {
        painter.setPen(Qt::gray);
        painter.drawText(rect(), Qt::AlignCenter, "无字符数据");
        return;
    }

    painter.setRenderHint(QPainter::Antialiasing, false);

    for (int i = 0; i < m_unicodeList.size(); i++) {
        int row = i / m_columns;
        int col = i % m_columns;
        int x = col * m_cellSize + 10;
        int y = row * m_cellSize + 10;

        QRect cellRect(x, y, m_cellSize, m_cellSize);

        if (i == m_selectedIndex) {
            painter.fillRect(cellRect, QColor(100, 150, 255, 100));
        }

        painter.setPen(QColor(200, 200, 200));
        painter.drawRect(cellRect);

        uint32_t unicode = m_unicodeList[i];
        const LvglGlyph &glyph = m_font.glyphs[unicode];

        QImage glyphImage = renderGlyph(glyph);
        if (!glyphImage.isNull()) {
            int imgX = x + (m_cellSize - glyphImage.width()) / 2;
            int imgY = y + (m_cellSize - glyphImage.height()) / 2;
            painter.drawImage(imgX, imgY, glyphImage);
        }
    }
}

void FontGridWidget::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        int col = (event->pos().x() - 10) / m_cellSize;
        int row = (event->pos().y() - 10) / m_cellSize;
        int index = row * m_columns + col;

        if (index >= 0 && index < m_unicodeList.size()) {
            m_selectedIndex = index;
            emit glyphSelected(m_unicodeList[index]);
            update();
        }
    }
}

QImage FontGridWidget::renderGlyph(const LvglGlyph &glyph)
{
    if (glyph.width == 0 || glyph.height == 0 || glyph.bitmap.isEmpty()) {
        return QImage();
    }

    QImage image(glyph.width, glyph.height, QImage::Format_RGB32);
    image.fill(Qt::white);

    int bpp = glyph.bpp;
    int readBpp = (bpp == 3 && glyph.use4BitAlignment) ? 4 : bpp;
    int maxValue = (1 << bpp) - 1;
    int pixelsPerByte = 8 / bpp;

    int pixelIndex = 0;
    for (int y = 0; y < glyph.height; y++) {
        for (int x = 0; x < glyph.width; x++) {
            int bitPos = pixelIndex * readBpp;
            int byteIndex = bitPos / 8;
            int bitOffset = bitPos % 8;

            if (byteIndex >= glyph.bitmap.size()) {
                break;
            }

            int pixelValue;
            int bitsInFirstByte = 8 - bitOffset;

            if (bitsInFirstByte >= readBpp) {
                int shift = bitsInFirstByte - readBpp;
                pixelValue = (glyph.bitmap[byteIndex] >> shift) & maxValue;
            } else {
                int bitsFromFirstByte = bitsInFirstByte;
                int bitsFromSecondByte = readBpp - bitsFromFirstByte;

                int firstPart = (glyph.bitmap[byteIndex] & ((1 << bitsFromFirstByte) - 1)) << bitsFromSecondByte;
                int secondPart = 0;
                if (byteIndex + 1 < glyph.bitmap.size()) {
                    secondPart = glyph.bitmap[byteIndex + 1] >> (8 - bitsFromSecondByte);
                }
                pixelValue = (firstPart | secondPart) & maxValue;
            }

            int grayValue = 255 - (pixelValue * 255) / maxValue;
            image.setPixelColor(x, y, QColor(grayValue, grayValue, grayValue));

            pixelIndex++;
        }
    }

    return image;
}
