#include "fontpreviewwidget.h"
#include <QPainter>
#include <QWheelEvent>
#include <QtMath>

FontPreviewWidget::FontPreviewWidget(QWidget *parent)
    : QWidget(parent)
    , m_hasGlyph(false)
    , m_scale(8)
{
    setMinimumSize(200, 200);
}

void FontPreviewWidget::setGlyph(const LvglGlyph &glyph)
{
    m_glyph = glyph;
    m_hasGlyph = true;
    update();
}

void FontPreviewWidget::clear()
{
    m_hasGlyph = false;
    update();
}

void FontPreviewWidget::paintEvent(QPaintEvent *event)
{
    QPainter painter(this);
    painter.fillRect(rect(), Qt::white);

    if (!m_hasGlyph) {
        painter.setPen(Qt::gray);
        painter.drawText(rect(), Qt::AlignCenter, "请选择一个字符");
        return;
    }

    QImage glyphImage = renderGlyph(m_glyph);
    if (glyphImage.isNull()) {
        painter.setPen(Qt::gray);
        painter.drawText(rect(), Qt::AlignCenter, "无效的字符数据");
        return;
    }

    QImage scaledImage = glyphImage.scaled(
        glyphImage.width() * m_scale,
        glyphImage.height() * m_scale,
        Qt::KeepAspectRatio,
        Qt::FastTransformation
    );

    int x = (width() - scaledImage.width()) / 2;
    int y = (height() - scaledImage.height()) / 2;

    painter.setRenderHint(QPainter::Antialiasing, false);
    painter.drawImage(x, y, scaledImage);

    painter.setPen(QColor(200, 200, 200));
    for (int px = 0; px <= m_glyph.width; px++) {
        int lineX = x + px * m_scale;
        painter.drawLine(lineX, y, lineX, y + scaledImage.height());
    }
    for (int py = 0; py <= m_glyph.height; py++) {
        int lineY = y + py * m_scale;
        painter.drawLine(x, lineY, x + scaledImage.width(), lineY);
    }

    painter.setPen(Qt::black);
    QString info = QString("尺寸: %1x%2 | 缩放: %3x | BPP: %4")
        .arg(m_glyph.width)
        .arg(m_glyph.height)
        .arg(m_scale)
        .arg(m_glyph.bpp);
    painter.drawText(10, height() - 10, info);
}

void FontPreviewWidget::wheelEvent(QWheelEvent *event)
{
    if (m_hasGlyph) {
        int delta = event->angleDelta().y();
        if (delta > 0) {
            m_scale = qMin(32, m_scale + 1);
        } else if (delta < 0) {
            m_scale = qMax(1, m_scale - 1);
        }
        update();
    }
}

QImage FontPreviewWidget::renderGlyph(const LvglGlyph &glyph)
{
    if (glyph.width == 0 || glyph.height == 0 || glyph.bitmap.isEmpty()) {
        return QImage();
    }

    QImage image(glyph.width, glyph.height, QImage::Format_RGB32);
    image.fill(Qt::white);

    int bpp = glyph.bpp;
    int readBpp = (bpp == 3 && glyph.use4BitAlignment) ? 4 : bpp;
    int maxValue = (1 << bpp) - 1;

    qDebug() << "Rendering glyph: width=" << glyph.width << "height=" << glyph.height
             << "bpp=" << bpp << "bitmap size=" << glyph.bitmap.size();

    if (glyph.unicode == 0x21) {
        qDebug() << "Rendering glyph '!':";
        qDebug() << "  First 8 bytes:";
        for (int i = 0; i < qMin(8, glyph.bitmap.size()); i++) {
            qDebug() << "    [" << i << "] = 0x" << QString::number((uint8_t)glyph.bitmap[i], 16);
        }
        qDebug() << "  First 10 pixels extracted:";
        for (int p = 0; p < 10; p++) {
            int byteIndex = (p * bpp) / 8;
            int bitShift = 8 - ((p * bpp) % 8) - bpp;
            int pixelValue = (glyph.bitmap[byteIndex] >> bitShift) & maxValue;
            qDebug() << "    Pixel" << p << "= byte[" << byteIndex << "] >> " << bitShift << "= " << pixelValue;
        }
    }

    int pixelIndex = 0;
    for (int y = 0; y < glyph.height; y++) {
        for (int x = 0; x < glyph.width; x++) {
            int bitPos = pixelIndex * readBpp;
            int byteIndex = bitPos / 8;
            int bitOffset = bitPos % 8;

            if (byteIndex >= glyph.bitmap.size()) {
                qDebug() << "Out of bounds at pixel" << pixelIndex << "byte" << byteIndex;
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
