#include "colorselectorwidget.h"
#include <QPainter>
#include <QMouseEvent>
#include <QtMath>

ColorSelectorWidget::ColorSelectorWidget(int bpp, QWidget *parent)
    : QWidget(parent)
    , m_bpp(bpp)
    , m_selectedColor((1 << bpp) - 1)
    , m_cellSize(24)
    , m_colsPerRow(16)
    , m_collapsed(true)
    , m_iconSize(32)
{
    updateSize();
    setCursor(Qt::PointingHandCursor);
}

void ColorSelectorWidget::setBpp(int bpp)
{
    m_bpp = bpp;
    int numColors = 1 << bpp;
    m_selectedColor = numColors - 1;
    updateSize();
    update();
}

void ColorSelectorWidget::setCollapsed(bool collapsed)
{
    m_collapsed = collapsed;
    updateSize();
    update();
}

void ColorSelectorWidget::updateSize()
{
    if (m_collapsed) {
        setFixedSize(m_iconSize, m_iconSize);
    } else {
        int numColors = 1 << m_bpp;
        int rows = (numColors + m_colsPerRow - 1) / m_colsPerRow;
        setFixedSize(m_cellSize * qMin(numColors, m_colsPerRow), m_cellSize * rows);
    }
}

void ColorSelectorWidget::paintEvent(QPaintEvent *event)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, false);

    if (m_collapsed) {
        int numColors = 1 << m_bpp;
        int maxValue = numColors - 1;
        int grayValue = 255 - (m_selectedColor * 255) / maxValue;

        QRect colorRect(2, 2, m_iconSize - 4, m_iconSize - 4);
        painter.fillRect(colorRect, QColor(grayValue, grayValue, grayValue));
        painter.setPen(QPen(Qt::black, 2));
        painter.drawRect(colorRect);

        painter.setPen(grayValue > 128 ? Qt::black : Qt::white);
        QFont font = painter.font();
        font.setPointSize(10);
        font.setBold(true);
        painter.setFont(font);
        painter.drawText(colorRect, Qt::AlignCenter, QString::number(m_selectedColor));
    } else {
        int numColors = 1 << m_bpp;
        int maxValue = numColors - 1;

        for (int i = 0; i < numColors; i++) {
            int row = i / m_colsPerRow;
            int col = i % m_colsPerRow;

            int grayValue = 255 - (i * 255) / maxValue;
            QRect cellRect(col * m_cellSize, row * m_cellSize, m_cellSize, m_cellSize);

            painter.fillRect(cellRect, QColor(grayValue, grayValue, grayValue));

            if (i == m_selectedColor) {
                painter.setPen(QPen(Qt::red, 3));
            } else {
                painter.setPen(QPen(Qt::black, 1));
            }
            painter.drawRect(cellRect);

            painter.setPen(grayValue > 128 ? Qt::black : Qt::white);
            QFont font = painter.font();
            font.setPointSize(8);
            painter.setFont(font);
            painter.drawText(cellRect, Qt::AlignCenter, QString::number(i));
        }
    }
}

void ColorSelectorWidget::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        if (m_collapsed) {
            setCollapsed(false);
        } else {
            int col = event->pos().x() / m_cellSize;
            int row = event->pos().y() / m_cellSize;
            int index = row * m_colsPerRow + col;
            int numColors = 1 << m_bpp;

            if (index >= 0 && index < numColors) {
                m_selectedColor = index;
                emit colorSelected(m_selectedColor);
                setCollapsed(true);
            }
        }
    }
}
