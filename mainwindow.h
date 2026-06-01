#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QLabel>
#include <QStackedWidget>
#include <QPushButton>
#include <QSpinBox>
#include "lvglfontparser.h"
#include "fontgridwidget.h"
#include "fontpreviewwidget.h"
#include "fonteditorwidget.h"
#include "colorselectorwidget.h"

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void openFile();
    void saveFile();
    void saveFileAs();
    void onGlyphSelected(uint32_t unicode);
    void onEditModeToggled(bool editMode);
    void onBrushSizeChanged(int size);
    void onColorSelected(int value);
    void onCoordinateChanged(int x, int y, int relX, int relY);
    void onUndo();
    void onRedo();
    void onGlyphModified();

private:
    void setupUI();
    void loadFont(const QString &filePath);
    void updateUndoRedoButtons();
    bool saveToFile(const QString &filePath);

    LvglFontParser m_parser;
    LvglFont m_currentFont;
    QString m_currentFilePath;
    FontGridWidget *m_gridWidget;
    FontPreviewWidget *m_previewWidget;
    FontEditorWidget *m_editorWidget;
    ColorSelectorWidget *m_colorSelector;
    QStackedWidget *m_stackedWidget;

    QPushButton *m_editButton;
    QPushButton *m_undoButton;
    QPushButton *m_redoButton;
    QSpinBox *m_brushSizeSpinBox;
    QLabel *m_coordLabel;
    QLabel *m_statusLabel;
    QLabel *m_infoLabel;

    uint32_t m_currentUnicode;
};

#endif
