#ifndef COLORSELECTORWIDGET_H
#define COLORSELECTORWIDGET_H

#include <QWidget>

class ColorSelectorWidget : public QWidget
{
    Q_OBJECT

public:
    explicit ColorSelectorWidget(int bpp, QWidget *parent = nullptr);

    void setBpp(int bpp);
    int getSelectedColor() const { return m_selectedColor; }
    void setCollapsed(bool collapsed);
    bool isCollapsed() const { return m_collapsed; }

signals:
    void colorSelected(int value);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;

private:
    void updateSize();

    int m_bpp;
    int m_selectedColor;
    int m_cellSize;
    int m_colsPerRow;
    bool m_collapsed;
    int m_iconSize;
};

#endif
