#include "mainwindow.h"
#include <QMenuBar>
#include <QMenu>
#include <QAction>
#include <QFileDialog>
#include <QMessageBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSplitter>
#include <QGroupBox>
#include <QScrollArea>
#include <QStatusBar>
#include <QToolBar>
#include <QComboBox>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_currentUnicode(0)
{
    setupUI();
}

MainWindow::~MainWindow()
{
}

void MainWindow::setupUI()
{
    QMenu *fileMenu = menuBar()->addMenu("文件(&F)");
    QAction *openAction = fileMenu->addAction("打开字库(&O)...");
    openAction->setShortcut(QKeySequence::Open);
    connect(openAction, &QAction::triggered, this, &MainWindow::openFile);

    QAction *saveAction = fileMenu->addAction("保存(&S)");
    saveAction->setShortcut(QKeySequence::Save);
    connect(saveAction, &QAction::triggered, this, &MainWindow::saveFile);

    QAction *saveAsAction = fileMenu->addAction("另存为(&A)...");
    saveAsAction->setShortcut(QKeySequence::SaveAs);
    connect(saveAsAction, &QAction::triggered, this, &MainWindow::saveFileAs);

    fileMenu->addSeparator();
    QAction *exitAction = fileMenu->addAction("退出(&X)");
    exitAction->setShortcut(QKeySequence::Quit);
    connect(exitAction, &QAction::triggered, this, &QWidget::close);

    QToolBar *toolbar = addToolBar("工具栏");
    toolbar->setMovable(false);

    m_editButton = new QPushButton("编辑模式");
    m_editButton->setCheckable(true);
    m_editButton->setEnabled(false);
    connect(m_editButton, &QPushButton::toggled, this, &MainWindow::onEditModeToggled);
    toolbar->addWidget(m_editButton);

    toolbar->addSeparator();

    toolbar->addWidget(new QLabel(" 画笔大小: "));
    m_brushSizeSpinBox = new QSpinBox();
    m_brushSizeSpinBox->setRange(1, 10);
    m_brushSizeSpinBox->setValue(1);
    m_brushSizeSpinBox->setEnabled(false);
    connect(m_brushSizeSpinBox, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &MainWindow::onBrushSizeChanged);
    toolbar->addWidget(m_brushSizeSpinBox);

    toolbar->addSeparator();

    toolbar->addWidget(new QLabel(" 绘制模式: "));
    QComboBox *drawModeCombo = new QComboBox();
    drawModeCombo->addItem("画笔", (int)DrawMode::Brush);
    drawModeCombo->addItem("直线", (int)DrawMode::Line);
    drawModeCombo->setEnabled(false);
    connect(drawModeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), [this, drawModeCombo](int index) {
        m_editorWidget->setDrawMode((DrawMode)drawModeCombo->itemData(index).toInt());
    });
    connect(m_editButton, &QPushButton::toggled, drawModeCombo, &QComboBox::setEnabled);
    toolbar->addWidget(drawModeCombo);

    toolbar->addSeparator();

    m_undoButton = new QPushButton("撤销");
    m_undoButton->setShortcut(QKeySequence::Undo);
    m_undoButton->setEnabled(false);
    connect(m_undoButton, &QPushButton::clicked, this, &MainWindow::onUndo);
    toolbar->addWidget(m_undoButton);

    m_redoButton = new QPushButton("重做");
    m_redoButton->setShortcut(QKeySequence::Redo);
    m_redoButton->setEnabled(false);
    connect(m_redoButton, &QPushButton::clicked, this, &MainWindow::onRedo);
    toolbar->addWidget(m_redoButton);

    toolbar->addSeparator();

    QPushButton *fitButton = new QPushButton("适配视图");
    fitButton->setEnabled(false);
    connect(fitButton, &QPushButton::clicked, [this]() {
        m_editorWidget->fitToView();
    });
    connect(m_editButton, &QPushButton::toggled, fitButton, &QPushButton::setEnabled);
    toolbar->addWidget(fitButton);

    toolbar->addSeparator();

    m_coordLabel = new QLabel("坐标: - ");
    toolbar->addWidget(m_coordLabel);

    QWidget *centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);

    QVBoxLayout *mainLayout = new QVBoxLayout(centralWidget);

    m_infoLabel = new QLabel("请打开一个LVGL字库文件 (.c 或 .bin)");
    m_infoLabel->setStyleSheet("QLabel { padding: 5px; background-color: #f0f0f0; }");
    mainLayout->addWidget(m_infoLabel);

    QSplitter *splitter = new QSplitter(Qt::Horizontal);
    mainLayout->addWidget(splitter, 1);

    QScrollArea *gridScrollArea = new QScrollArea();
    gridScrollArea->setWidgetResizable(true);
    m_gridWidget = new FontGridWidget();
    gridScrollArea->setWidget(m_gridWidget);
    connect(m_gridWidget, &FontGridWidget::glyphSelected, this, &MainWindow::onGlyphSelected);

    QGroupBox *gridBox = new QGroupBox("字符列表");
    QVBoxLayout *gridBoxLayout = new QVBoxLayout(gridBox);
    gridBoxLayout->addWidget(gridScrollArea);
    splitter->addWidget(gridBox);

    QGroupBox *rightBox = new QGroupBox("字符预览/编辑");
    QVBoxLayout *rightBoxLayout = new QVBoxLayout(rightBox);

    m_colorSelector = new ColorSelectorWidget(4);
    m_colorSelector->setVisible(false);
    connect(m_colorSelector, &ColorSelectorWidget::colorSelected,
            this, &MainWindow::onColorSelected);
    rightBoxLayout->addWidget(m_colorSelector);

    m_stackedWidget = new QStackedWidget();

    m_previewWidget = new FontPreviewWidget();
    m_stackedWidget->addWidget(m_previewWidget);

    m_editorWidget = new FontEditorWidget();
    connect(m_editorWidget, &FontEditorWidget::coordinateChanged,
            this, &MainWindow::onCoordinateChanged);
    connect(m_editorWidget, &FontEditorWidget::glyphModified,
            this, &MainWindow::onGlyphModified);
    m_stackedWidget->addWidget(m_editorWidget);

    rightBoxLayout->addWidget(m_stackedWidget, 1);
    splitter->addWidget(rightBox);

    splitter->setStretchFactor(0, 2);
    splitter->setStretchFactor(1, 1);

    m_statusLabel = new QLabel("就绪");
    statusBar()->addWidget(m_statusLabel);
}

void MainWindow::openFile()
{
    QString filePath = QFileDialog::getOpenFileName(
        this,
        "打开LVGL字库文件",
        QString(),
        "LVGL字库文件 (*.c *.bin);;C源文件 (*.c);;二进制文件 (*.bin);;所有文件 (*.*)"
    );

    if (!filePath.isEmpty()) {
        loadFont(filePath);
    }
}

void MainWindow::loadFont(const QString &filePath)
{
    m_statusLabel->setText("正在加载字库...");

    if (m_parser.parseFile(filePath)) {
        m_currentFont = m_parser.getFont();
        m_currentFilePath = filePath;
        m_gridWidget->setFont(m_currentFont);
        m_previewWidget->clear();
        m_editorWidget->clear();
        m_editButton->setChecked(false);
        m_editButton->setEnabled(false);

        m_colorSelector->setBpp(m_currentFont.bpp);

        QString info = QString("字库: %1 | 字符数: %2 | 行高: %3 | BPP: %4")
            .arg(QFileInfo(filePath).fileName())
            .arg(m_currentFont.glyphs.size())
            .arg(m_currentFont.lineHeight)
            .arg(m_currentFont.bpp);
        m_infoLabel->setText(info);
        m_statusLabel->setText("字库加载成功");
    } else {
        QMessageBox::critical(this, "错误", "加载字库失败: " + m_parser.getError());
        m_statusLabel->setText("加载失败");
    }
}

bool MainWindow::saveToFile(const QString &filePath)
{
    m_statusLabel->setText("正在保存字库...");

    if (m_parser.saveToFile(filePath, m_currentFont)) {
        m_currentFilePath = filePath;
        m_statusLabel->setText("字库保存成功");
        return true;
    } else {
        QMessageBox::critical(this, "错误", "保存字库失败: " + m_parser.getError());
        m_statusLabel->setText("保存失败");
        return false;
    }
}

void MainWindow::saveFile()
{
    if (m_currentFont.glyphs.isEmpty()) {
        QMessageBox::warning(this, "警告", "没有可保存的字库数据");
        return;
    }

    if (m_currentFilePath.isEmpty()) {
        saveFileAs();
        return;
    }
    QMessageBox::StandardButton reply = QMessageBox::question(
        this,
        "确认覆盖",
        "确定要覆盖文件吗？\n" + m_currentFilePath,
        QMessageBox::Yes | QMessageBox::No
    );

    if (reply == QMessageBox::Yes) {
        if (saveToFile(m_currentFilePath)) {
            QMessageBox::information(this, "成功", "字库已保存到: " + m_currentFilePath);
        }
    }
}

void MainWindow::saveFileAs()
{
    if (m_currentFont.glyphs.isEmpty()) {
        QMessageBox::warning(this, "警告", "没有可保存的字库数据");
        return;
    }

    QString filePath = QFileDialog::getSaveFileName(
        this,
        "另存为LVGL字库文件",
        m_currentFilePath.isEmpty() ? QString() : m_currentFilePath,
        "C源文件 (*.c);;二进制文件 (*.bin);;所有文件 (*.*)"
    );

    if (!filePath.isEmpty()) {
        if (saveToFile(filePath)) {
            QMessageBox::information(this, "成功", "字库已保存到: " + filePath);
        }
    }
}

void MainWindow::onGlyphSelected(uint32_t unicode)
{
    if (m_currentFont.glyphs.contains(unicode)) {
        m_currentUnicode = unicode;
        const LvglGlyph &glyph = m_currentFont.glyphs[unicode];

        m_previewWidget->setGlyph(glyph);
        m_editorWidget->setGlyph(glyph);
        m_editButton->setEnabled(true);

        QString charStr;
        if (unicode < 0x10000) {
            charStr = QString(QChar(unicode));
        } else {
            char32_t ch = unicode;
            charStr = QString::fromUcs4(&ch, 1);
        }

        m_statusLabel->setText(QString("选中字符: U+%1 '%2'")
            .arg(unicode, 4, 16, QChar('0')).toUpper()
            .arg(charStr));
    }
}

void MainWindow::onGlyphModified()
{
    m_currentFont.glyphs[m_currentUnicode] = m_editorWidget->getGlyph();
    m_gridWidget->setFont(m_currentFont);
    updateUndoRedoButtons();
}

void MainWindow::onEditModeToggled(bool editMode)
{
    if (editMode) {
        m_stackedWidget->setCurrentWidget(m_editorWidget);
        m_colorSelector->setVisible(true);
        m_brushSizeSpinBox->setEnabled(true);
        m_editorWidget->setBrushColor(m_colorSelector->getSelectedColor());
    } else {
        m_stackedWidget->setCurrentWidget(m_previewWidget);
        m_colorSelector->setVisible(false);
        m_brushSizeSpinBox->setEnabled(false);
        m_coordLabel->setText("坐标: - ");
    }
    updateUndoRedoButtons();
}

void MainWindow::onBrushSizeChanged(int size)
{
    m_editorWidget->setBrushSize(size);
}

void MainWindow::onColorSelected(int value)
{
    m_editorWidget->setBrushColor(value);
}

void MainWindow::onCoordinateChanged(int x, int y, int relX, int relY)
{
    m_coordLabel->setText(QString("坐标: (%1, %2) | 相对: (%3, %4)")
        .arg(x).arg(y).arg(relX).arg(relY));
}

void MainWindow::onUndo()
{
    m_editorWidget->undo();
    updateUndoRedoButtons();
}

void MainWindow::onRedo()
{
    m_editorWidget->redo();
    updateUndoRedoButtons();
}

void MainWindow::updateUndoRedoButtons()
{
    m_undoButton->setEnabled(m_editButton->isChecked() && m_editorWidget->canUndo());
    m_redoButton->setEnabled(m_editButton->isChecked() && m_editorWidget->canRedo());
}
