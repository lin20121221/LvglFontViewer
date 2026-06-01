#ifndef FONTPREVIEWWIDGET_H
#define FONTPREVIEWWIDGET_H

#include <QWidget>
#include "lvglfontparser.h"

class FontPreviewWidget : public QWidget
{
    Q_OBJECT

public:
    explicit FontPreviewWidget(QWidget *parent = nullptr);

    void setGlyph(const LvglGlyph &glyph);
    void clear();

protected:
    void paintEvent(QPaintEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;

private:
    QImage renderGlyph(const LvglGlyph &glyph);

    LvglGlyph m_glyph;
    bool m_hasGlyph;
    int m_scale;
};

#endif
