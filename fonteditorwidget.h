#ifndef FONTEDITORWIDGET_H
#define FONTEDITORWIDGET_H

#include <QWidget>
#include <QVector>
#include <QPoint>
#include <QStack>
#include "lvglfontparser.h"

enum class DrawMode {
    Brush,
    Line
};

class FontEditorWidget : public QWidget
{
    Q_OBJECT

public:
    explicit FontEditorWidget(QWidget *parent = nullptr);

    void setGlyph(const LvglGlyph &glyph);
    void clear();
    LvglGlyph getGlyph() const;

    void setBrushSize(int size);
    void setBrushColor(int value);
    void setDrawMode(DrawMode mode);

    void undo();
    void redo();
    bool canUndo() const { return !m_undoStack.isEmpty(); }
    bool canRedo() const { return !m_redoStack.isEmpty(); }

    void fitToView();

signals:
    void glyphModified();
    void coordinateChanged(int x, int y, int relX, int relY);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void leaveEvent(QEvent *event) override;

private:
    void drawPixel(int x, int y);
    void drawLine(int x0, int y0, int x1, int y1);
    void drawLineAntialiased(int x0, int y0, int x1, int y1);
    void saveState();
    QPoint pixelFromPos(const QPoint &pos);
    QPoint constrainToAxis(const QPoint &current, const QPoint &start);

    LvglGlyph m_glyph;
    QVector<uint8_t> m_pixelData;
    bool m_hasGlyph;
    int m_scale;
    int m_brushSize;
    int m_brushColor;
    DrawMode m_drawMode;

    bool m_drawing;
    QPoint m_lastPixel;
    QPoint m_drawStartPixel;
    QPoint m_currentMousePixel;
    bool m_mouseInWidget;

    // Pan support
    bool m_panning;
    QPoint m_panStart;
    QPoint m_offset;

    QStack<QVector<uint8_t>> m_undoStack;
    QStack<QVector<uint8_t>> m_redoStack;
};

#endif
