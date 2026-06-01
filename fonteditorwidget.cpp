#include "fonteditorwidget.h"
#include <QPainter>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QtMath>

FontEditorWidget::FontEditorWidget(QWidget *parent)
    : QWidget(parent)
    , m_hasGlyph(false)
    , m_scale(16)
    , m_brushSize(1)
    , m_brushColor(15)
    , m_drawMode(DrawMode::Brush)
    , m_drawing(false)
    , m_mouseInWidget(false)
    , m_panning(false)
    , m_offset(0, 0)
{
    setMinimumSize(400, 400);
    setMouseTracking(true);
}

void FontEditorWidget::setGlyph(const LvglGlyph &glyph)
{
    m_glyph = glyph;
    m_hasGlyph = true;
    m_undoStack.clear();
    m_redoStack.clear();

    int totalPixels = glyph.width * glyph.height;
    m_pixelData.resize(totalPixels);

    int bpp = glyph.bpp;
    int readBpp = (bpp == 3 && glyph.use4BitAlignment) ? 4 : bpp;
    int maxValue = (1 << bpp) - 1;

    for (int i = 0; i < totalPixels; i++) {
        int bitPos = i * readBpp;
        int byteIndex = bitPos / 8;
        int bitOffset = bitPos % 8;

        if (byteIndex < glyph.bitmap.size()) {
            int bitsInFirstByte = 8 - bitOffset;

            if (bitsInFirstByte >= readBpp) {
                int shift = bitsInFirstByte - readBpp;
                m_pixelData[i] = (glyph.bitmap[byteIndex] >> shift) & maxValue;
            } else {
                int bitsFromFirstByte = bitsInFirstByte;
                int bitsFromSecondByte = readBpp - bitsFromFirstByte;

                int firstPart = (glyph.bitmap[byteIndex] & ((1 << bitsFromFirstByte) - 1)) << bitsFromSecondByte;
                int secondPart = 0;
                if (byteIndex + 1 < glyph.bitmap.size()) {
                    secondPart = glyph.bitmap[byteIndex + 1] >> (8 - bitsFromSecondByte);
                }
                m_pixelData[i] = (firstPart | secondPart) & maxValue;
            }
        } else {
            m_pixelData[i] = 0;
        }
    }

    update();
}

void FontEditorWidget::clear()
{
    m_hasGlyph = false;
    m_pixelData.clear();
    m_undoStack.clear();
    m_redoStack.clear();
    m_offset = QPoint(0, 0);
    update();
}

void FontEditorWidget::fitToView()
{
    if (!m_hasGlyph) return;

    int padding = 40;
    int availableWidth = width() - padding * 2;
    int availableHeight = height() - padding * 2;

    int scaleX = availableWidth / m_glyph.width;
    int scaleY = availableHeight / m_glyph.height;

    m_scale = qMax(1, qMin(scaleX, scaleY));
    m_offset = QPoint(0, 0);
    update();
}

LvglGlyph FontEditorWidget::getGlyph() const
{
    LvglGlyph glyph = m_glyph;

    int bpp = glyph.bpp;
    int writeBpp = (bpp == 3 && glyph.use4BitAlignment) ? 4 : bpp;
    int totalPixels = glyph.width * glyph.height;
    int totalBytes = (totalPixels * writeBpp + 7) / 8;

    glyph.bitmap.resize(totalBytes);
    glyph.bitmap.fill(0);

    for (int i = 0; i < totalPixels; i++) {
        int bitPos = i * writeBpp;
        int byteIndex = bitPos / 8;
        int bitOffset = bitPos % 8;
        int bitsInFirstByte = 8 - bitOffset;

        if (byteIndex < glyph.bitmap.size()) {
            if (bitsInFirstByte >= writeBpp) {
                int shift = bitsInFirstByte - writeBpp;
                glyph.bitmap[byteIndex] |= (m_pixelData[i] << shift);
            } else {
                int bitsFromSecondByte = writeBpp - bitsInFirstByte;
                glyph.bitmap[byteIndex] |= (m_pixelData[i] >> bitsFromSecondByte);
                if (byteIndex + 1 < glyph.bitmap.size()) {
                    glyph.bitmap[byteIndex + 1] |= (m_pixelData[i] << (8 - bitsFromSecondByte));
                }
            }
        }
    }

    return glyph;
}

void FontEditorWidget::setBrushSize(int size)
{
    m_brushSize = qMax(1, size);
}

void FontEditorWidget::setBrushColor(int value)
{
    int maxValue = (1 << m_glyph.bpp) - 1;
    m_brushColor = qBound(0, value, maxValue);
}

void FontEditorWidget::setDrawMode(DrawMode mode)
{
    m_drawMode = mode;
}

void FontEditorWidget::undo()
{
    if (!m_undoStack.isEmpty()) {
        m_redoStack.push(m_pixelData);
        m_pixelData = m_undoStack.pop();
        update();
        emit glyphModified();
    }
}

void FontEditorWidget::redo()
{
    if (!m_redoStack.isEmpty()) {
        m_undoStack.push(m_pixelData);
        m_pixelData = m_redoStack.pop();
        update();
        emit glyphModified();
    }
}

void FontEditorWidget::paintEvent(QPaintEvent *event)
{
    QPainter painter(this);
    painter.fillRect(rect(), QColor(240, 240, 240));

    if (!m_hasGlyph) {
        painter.setPen(Qt::gray);
        painter.drawText(rect(), Qt::AlignCenter, "请选择一个字符进行编辑");
        return;
    }

    int offsetX = (width() - m_glyph.width * m_scale) / 2 + m_offset.x();
    int offsetY = (height() - m_glyph.height * m_scale) / 2 + m_offset.y();

    painter.setRenderHint(QPainter::Antialiasing, false);

    int maxValue = (1 << m_glyph.bpp) - 1;
    for (int y = 0; y < m_glyph.height; y++) {
        for (int x = 0; x < m_glyph.width; x++) {
            int pixelIndex = y * m_glyph.width + x;
            int pixelValue = m_pixelData[pixelIndex];
            int grayValue = 255 - (pixelValue * 255) / maxValue;

            QRect pixelRect(offsetX + x * m_scale, offsetY + y * m_scale, m_scale, m_scale);
            painter.fillRect(pixelRect, QColor(grayValue, grayValue, grayValue));
        }
    }

    painter.setPen(QColor(200, 200, 200));
    for (int x = 0; x <= m_glyph.width; x++) {
        int lineX = offsetX + x * m_scale;
        painter.drawLine(lineX, offsetY, lineX, offsetY + m_glyph.height * m_scale);
    }
    for (int y = 0; y <= m_glyph.height; y++) {
        int lineY = offsetY + y * m_scale;
        painter.drawLine(offsetX, lineY, offsetX + m_glyph.width * m_scale, lineY);
    }

    if (m_mouseInWidget && m_currentMousePixel.x() >= 0 && m_currentMousePixel.x() < m_glyph.width &&
        m_currentMousePixel.y() >= 0 && m_currentMousePixel.y() < m_glyph.height) {

        painter.setPen(QPen(Qt::red, 2));
        for (int dy = 0; dy < m_brushSize; dy++) {
            for (int dx = 0; dx < m_brushSize; dx++) {
                int px = m_currentMousePixel.x() + dx;
                int py = m_currentMousePixel.y() + dy;
                if (px < m_glyph.width && py < m_glyph.height) {
                    QRect highlightRect(offsetX + px * m_scale, offsetY + py * m_scale, m_scale, m_scale);
                    painter.drawRect(highlightRect);
                }
            }
        }
    }
}

void FontEditorWidget::mousePressEvent(QMouseEvent *event)
{
    if (!m_hasGlyph) {
        return;
    }

    if (event->button() == Qt::MiddleButton) {
        m_panning = true;
        m_panStart = event->pos();
        setCursor(Qt::ClosedHandCursor);
        return;
    }

    if (event->button() != Qt::LeftButton) {
        return;
    }

    QPoint pixel = pixelFromPos(event->pos());
    if (pixel.x() >= 0 && pixel.x() < m_glyph.width &&
        pixel.y() >= 0 && pixel.y() < m_glyph.height) {

        saveState();
        m_drawing = true;
        m_drawStartPixel = pixel;
        m_lastPixel = pixel;

        if (m_drawMode == DrawMode::Brush) {
            drawPixel(pixel.x(), pixel.y());
        }
    }
}

void FontEditorWidget::mouseMoveEvent(QMouseEvent *event)
{
    if (!m_hasGlyph) {
        return;
    }

    if (m_panning) {
        QPoint delta = event->pos() - m_panStart;
        m_offset += delta;
        m_panStart = event->pos();
        update();
        return;
    }

    QPoint pixel = pixelFromPos(event->pos());
    m_currentMousePixel = pixel;
    m_mouseInWidget = true;

    if (pixel.x() >= 0 && pixel.x() < m_glyph.width &&
        pixel.y() >= 0 && pixel.y() < m_glyph.height) {

        int relX = pixel.x() - m_drawStartPixel.x();
        int relY = pixel.y() - m_drawStartPixel.y();
        emit coordinateChanged(pixel.x(), pixel.y(), relX, relY);
    }

    if (m_drawing && m_drawMode == DrawMode::Brush) {
        QPoint targetPixel = pixel;

        if (event->modifiers() & Qt::ControlModifier) {
            targetPixel = constrainToAxis(pixel, m_drawStartPixel);
        }

        if (targetPixel != m_lastPixel &&
            targetPixel.x() >= 0 && targetPixel.x() < m_glyph.width &&
            targetPixel.y() >= 0 && targetPixel.y() < m_glyph.height) {

            drawPixel(targetPixel.x(), targetPixel.y());
            m_lastPixel = targetPixel;
        }
    }

    update();
}

void FontEditorWidget::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        if (m_drawing && m_drawMode == DrawMode::Line) {
            QPoint pixel = pixelFromPos(event->pos());
            if (pixel.x() >= 0 && pixel.x() < m_glyph.width &&
                pixel.y() >= 0 && pixel.y() < m_glyph.height) {

                int dx = qAbs(pixel.x() - m_drawStartPixel.x());
                int dy = qAbs(pixel.y() - m_drawStartPixel.y());
                bool isDiagonal = (dx > 0 && dy > 0 && dx != dy);

                if (isDiagonal && m_glyph.bpp > 1) {
                    drawLineAntialiased(m_drawStartPixel.x(), m_drawStartPixel.y(),
                                       pixel.x(), pixel.y());
                } else {
                    drawLine(m_drawStartPixel.x(), m_drawStartPixel.y(),
                            pixel.x(), pixel.y());
                }
            }
        }
        m_drawing = false;
    } else if (event->button() == Qt::MiddleButton) {
        m_panning = false;
        setCursor(Qt::ArrowCursor);
    }
}

void FontEditorWidget::wheelEvent(QWheelEvent *event)
{
    if (m_hasGlyph) {
        int delta = event->angleDelta().y();
        if (delta > 0) {
            m_scale = qMin(32, m_scale + 1);
        } else if (delta < 0) {
            m_scale = qMax(4, m_scale - 1);
        }
        update();
    }
}

void FontEditorWidget::leaveEvent(QEvent *event)
{
    m_mouseInWidget = false;
    update();
}

void FontEditorWidget::drawPixel(int x, int y)
{
    for (int dy = 0; dy < m_brushSize; dy++) {
        for (int dx = 0; dx < m_brushSize; dx++) {
            int px = x + dx;
            int py = y + dy;
            if (px >= 0 && px < m_glyph.width && py >= 0 && py < m_glyph.height) {
                int pixelIndex = py * m_glyph.width + px;
                m_pixelData[pixelIndex] = m_brushColor;
            }
        }
    }
    update();
    emit glyphModified();
}

void FontEditorWidget::drawLine(int x0, int y0, int x1, int y1)
{
    int dx = qAbs(x1 - x0);
    int dy = qAbs(y1 - y0);
    int sx = (x0 < x1) ? 1 : -1;
    int sy = (y0 < y1) ? 1 : -1;
    int err = dx - dy;

    while (true) {
        drawPixel(x0, y0);

        if (x0 == x1 && y0 == y1) break;

        int e2 = 2 * err;
        if (e2 > -dy) {
            err -= dy;
            x0 += sx;
        }
        if (e2 < dx) {
            err += dx;
            y0 += sy;
        }
    }
}

void FontEditorWidget::drawLineAntialiased(int x0, int y0, int x1, int y1)
{
    auto ipart = [](float x) -> int { return (int)x; };
    auto fpart = [](float x) -> float { return x - (int)x; };
    auto rfpart = [&fpart](float x) -> float { return 1.0f - fpart(x); };

    auto plot = [this](int x, int y, float brightness) {
        if (x >= 0 && x < m_glyph.width && y >= 0 && y < m_glyph.height) {
            int pixelIndex = y * m_glyph.width + x;
            int maxValue = (1 << m_glyph.bpp) - 1;
            int newValue = (int)(m_brushColor * brightness + 0.5f);
            m_pixelData[pixelIndex] = qBound(0, newValue, maxValue);
        }
    };

    bool steep = qAbs(y1 - y0) > qAbs(x1 - x0);

    if (steep) {
        qSwap(x0, y0);
        qSwap(x1, y1);
    }
    if (x0 > x1) {
        qSwap(x0, x1);
        qSwap(y0, y1);
    }

    float dx = x1 - x0;
    float dy = y1 - y0;
    float gradient = (dx == 0) ? 1.0f : dy / dx;

    int xend = (int)(x0 + 0.5f);
    float yend = y0 + gradient * (xend - x0);
    float xgap = rfpart(x0 + 0.5f);
    int xpxl1 = xend;
    int ypxl1 = ipart(yend);

    if (steep) {
        plot(ypxl1, xpxl1, rfpart(yend) * xgap);
        plot(ypxl1 + 1, xpxl1, fpart(yend) * xgap);
    } else {
        plot(xpxl1, ypxl1, rfpart(yend) * xgap);
        plot(xpxl1, ypxl1 + 1, fpart(yend) * xgap);
    }

    float intery = yend + gradient;

    xend = (int)(x1 + 0.5f);
    yend = y1 + gradient * (xend - x1);
    xgap = fpart(x1 + 0.5f);
    int xpxl2 = xend;
    int ypxl2 = ipart(yend);

    if (steep) {
        plot(ypxl2, xpxl2, rfpart(yend) * xgap);
        plot(ypxl2 + 1, xpxl2, fpart(yend) * xgap);
    } else {
        plot(xpxl2, ypxl2, rfpart(yend) * xgap);
        plot(xpxl2, ypxl2 + 1, fpart(yend) * xgap);
    }

    if (steep) {
        for (int x = xpxl1 + 1; x < xpxl2; x++) {
            plot(ipart(intery), x, rfpart(intery));
            plot(ipart(intery) + 1, x, fpart(intery));
            intery += gradient;
        }
    } else {
        for (int x = xpxl1 + 1; x < xpxl2; x++) {
            plot(x, ipart(intery), rfpart(intery));
            plot(x, ipart(intery) + 1, fpart(intery));
            intery += gradient;
        }
    }

    update();
    emit glyphModified();
}

void FontEditorWidget::saveState()
{
    m_undoStack.push(m_pixelData);
    m_redoStack.clear();

    if (m_undoStack.size() > 50) {
        m_undoStack.removeFirst();
    }
}

QPoint FontEditorWidget::pixelFromPos(const QPoint &pos)
{
    int offsetX = (width() - m_glyph.width * m_scale) / 2 + m_offset.x();
    int offsetY = (height() - m_glyph.height * m_scale) / 2 + m_offset.y();

    int x = (pos.x() - offsetX) / m_scale;
    int y = (pos.y() - offsetY) / m_scale;

    return QPoint(x, y);
}

QPoint FontEditorWidget::constrainToAxis(const QPoint &current, const QPoint &start)
{
    int dx = qAbs(current.x() - start.x());
    int dy = qAbs(current.y() - start.y());

    if (dx > dy) {
        return QPoint(current.x(), start.y());
    } else {
        return QPoint(start.x(), current.y());
    }
}
