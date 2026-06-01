#ifndef FONTGRIDWIDGET_H
#define FONTGRIDWIDGET_H

#include <QWidget>
#include "lvglfontparser.h"

class FontGridWidget : public QWidget
{
    Q_OBJECT

public:
    explicit FontGridWidget(QWidget *parent = nullptr);

    void setFont(const LvglFont &font);
    void clear();

signals:
    void glyphSelected(uint32_t unicode);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    void calculateLayout();
    QImage renderGlyph(const LvglGlyph &glyph);

    LvglFont m_font;
    QVector<uint32_t> m_unicodeList;
    int m_cellSize;
    int m_columns;
    int m_selectedIndex;
};

#endif
