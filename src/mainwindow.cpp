#include "mainwindow.h"
#include <limits>
#include "sensor_manager.h"
#include "alarm_manager.h"
#include "bacnet_interface.h"
#include "config.h"

#include <QApplication>
#include <QScreen>
#include <QCursor>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QFrame>
#include <QTabWidget>
#include <QPixmap>
#include <QLineEdit>
#include <QVBoxLayout>
#include <QPushButton>
#include <QScrollArea>
#include <QComboBox>
#include <QtCharts/QChart>
#include <QtCharts/QChartView>
#include <QtCharts/QLineSeries>
#include <QtCharts/QValueAxis>
#include <QDialog>
#include <QUrl>
#include <qrencode.h>
#include <QSvgRenderer>
#include <QPainter>
#include <QPen>
#include <QBrush>
#include <QRadialGradient>
#include <random>
#include <QFile>
#include <QTextStream>
#include <QHeaderView>
#include <QScroller>
#include <QGestureEvent>
#include <QGraphicsPixmapItem>
#include <QStackedLayout>
#include <QMediaPlayer>
#include <QVideoWidget>
#include <QListWidget>
#include <QAudioOutput>
#include <poppler/qt6/poppler-qt6.h>

MainWindow::MainWindow(SensorManager& sensors, AlarmManager& alarms,
                       BACnetInterface& bacnet, QWidget* parent)
    : QMainWindow(parent)
    , sensorManager(sensors)
    , alarmManager(alarms)
    , bacnetInterface(bacnet) {

    const auto& displayConfig = Config::instance().getDisplayConfig();
    setWindowTitle("Notus One");
    setFixedSize(displayConfig.width, displayConfig.height);

    setupUI();

    // Hide cursor for embedded kiosk mode (transparent pixmap works on eglfs/linuxfb
    // where Qt::BlankCursor is ignored)
    if (!displayConfig.devMode) {
        QPixmap cursorPixmap(1, 1);
        cursorPixmap.fill(Qt::transparent);
        QApplication::setOverrideCursor(QCursor(cursorPixmap));
    }

    // Update timer for display (1 Hz)
    updateTimer = new QTimer(this);
    connect(updateTimer, &QTimer::timeout, this, &MainWindow::onUpdateTimer);
    updateTimer->start(1000);

    // BACnet processing timer (10 ms for responsive message handling)
    bacnetTimer = new QTimer(this);
    connect(bacnetTimer, &QTimer::timeout, this, &MainWindow::onBACnetTimer);
    bacnetTimer->start(10);
}

MainWindow::~MainWindow() {
}

static QPixmap bellPixmap(const QColor& color, int size) {
    static const char* svg =
        "<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 24 24'>"
        "<path d='M12 22c1.1 0 2-.9 2-2h-4c0 1.1.9 2 2 2z"
        "M18 16V11c0-3.07-1.64-5.64-4.5-6.32V4c0-.83-.67-1.5-1.5-1.5"
        "s-1.5.67-1.5 1.5v.68C7.63 5.36 6 7.92 6 11v5l-2 2v1h16v-1l-2-2z'/>"
        "</svg>";
    QByteArray svgData(svg);
    QSvgRenderer renderer(svgData);
    QPixmap px(size, size);
    px.fill(Qt::transparent);
    QPainter painter(&px);
    renderer.render(&painter);
    painter.setCompositionMode(QPainter::CompositionMode_SourceIn);
    painter.fillRect(px.rect(), color);
    return px;
}

// Central widget that paints the gradient+grain backdrop for all transparent children
namespace {
class BgWidget : public QWidget {
    QPixmap m_px;
public:
    BgWidget(const QPixmap& px, QWidget* parent = nullptr) : QWidget(parent), m_px(px) {}
    void paintEvent(QPaintEvent*) override { QPainter(this).drawPixmap(rect(), m_px); }
};
} // namespace

void MainWindow::setupUI() {
    const auto& displayConfig = Config::instance().getDisplayConfig();

    // Pre-generate background: flat navy base + soft bloom patches + vignette + dense grain
    // Use actual screen size so the pixmap is always 1:1 (no stretching)
    const QRect screenRect = QGuiApplication::primaryScreen()->geometry();
    const int W = screenRect.width(), H = screenRect.height();
    m_bgPixmap = QPixmap(W, H);
    {
        QPainter p(&m_bgPixmap);
        p.setRenderHint(QPainter::Antialiasing);

        std::mt19937 rng(42);

        // 1. Flat dark-navy base
        p.fillRect(m_bgPixmap.rect(), QColor(0x16, 0x16, 0x32));



        // 4. Dense film grain — two passes so grain reads over both the dark
        //    and light areas, integrating with the bloom patches below
        std::uniform_int_distribution<int> rx(0, W - 1);
        std::uniform_int_distribution<int> ry(0, H - 1);
        std::uniform_int_distribution<int> ra(2, 22);
        std::uniform_int_distribution<int> rb(0, 2);   // 0=dark, 1-2=light

        // 3. Vignette: transparent centre → near-black corners
        double halfDiag = qSqrt(double(W * W + H * H)) / 2.0;
        QRadialGradient vignette(W * 0.5, H * 0.5, halfDiag);
        vignette.setColorAt(0.0,  QColor(0, 0, 0,   0));
        vignette.setColorAt(0.40, QColor(0, 0, 0,   0));
        vignette.setColorAt(0.70, QColor(0, 0, 0,  85));
        vignette.setColorAt(1.0,  QColor(0, 0, 0, 185));
        p.fillRect(m_bgPixmap.rect(), vignette);

        const int grainCount = W * H / 30;  // ~1 dot per 30 pixels, scales with resolution
        for (int i = 0; i < grainCount; i++) {
            int x = rx(rng), y = ry(rng), a = ra(rng);
            int kind = rb(rng);
            if (kind == 0)
                p.setPen(QColor(0, 0, 8, a + 10));           // dark speck
            else if (kind == 1)
                p.setPen(QColor(185, 195, 235, a));           // mid light speck
            else
                p.setPen(QColor(220, 228, 255, a / 2 + 2));  // bright highlight speck
            p.drawPoint(x, y);
        }
    }

    BgWidget* central = new BgWidget(m_bgPixmap, this);
    setCentralWidget(central);

    QVBoxLayout* mainLayout = new QVBoxLayout(central);

    // Tabs
    tabWidget = new QTabWidget();

    // Modern HMI styling
    tabWidget->setStyleSheet(R"(
        QTabWidget, QTabBar {
            background: transparent;
        }
        QTabWidget::pane {
            border: 1px solid #2a2a52;
            background: transparent;
            border-radius: 8px;
        }
        QTabBar::tab {
            background: #10102c;
            color: #a0a0a0;
            padding: 18px 36px;
            margin-right: 2px;
            border-top-left-radius: 6px;
            border-top-right-radius: 6px;
            font-size: 20px;
            font-weight: bold;
        }
        QTabBar::tab:selected {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                stop:0 #22224a, stop:1 #141428);
            color: white;
            border-bottom: 2px solid white;
        }
        QTabBar::tab:hover:!selected {
            background: #1e1e40;
            color: #e0e0e0;
        }
    )");

    // Set overall window style
    central->setStyleSheet(R"(
        QWidget {
            background: transparent;
            color: #e0e0e0;
            font-family: 'Segoe UI', Arial, sans-serif;
        }
        *:focus {
            outline: none;
        }
        QLabel {
            background: transparent;
            color: #e0e0e0;
        }
        QGroupBox {
            background: transparent;
            border: 1px solid #2a2a52;
            border-radius: 6px;
            margin-top: 12px;
            padding-top: 12px;
        }
        QGroupBox::title {
            subcontrol-origin: margin;
            left: 10px;
            padding: 0 5px;
        }
        QPushButton {
            background: #18183a;
            color: #e0e0e0;
            border: 1px solid #2a2a52;
            border-radius: 4px;
            padding: 8px 16px;
            font-weight: bold;
        }
        QPushButton:hover {
            background: #21213e;
            border-color: #00d4ff;
        }
        QPushButton:pressed {
            background: #141428;
        }
        QComboBox {
            background: #18183a;
            color: #e0e0e0;
            border: 1px solid #2a2a52;
            border-radius: 4px;
            padding: 6px 12px;
        }
        QComboBox:hover {
            border-color: #00d4ff;
        }
        QComboBox::drop-down {
            border: none;
        }
        QComboBox QAbstractItemView {
            background: #18183a;
            color: #e0e0e0;
            selection-background-color: #1e2044;
        }
        QScrollBar:vertical {
            background: transparent;
            width: 8px;
            margin: 0;
        }
        QScrollBar::handle:vertical {
            background: #2a2a52;
            border-radius: 4px;
            min-height: 30px;
        }
        QScrollBar::handle:vertical:hover {
            background: #00d4ff;
        }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical,
        QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical {
            background: transparent;
            height: 0px;
        }
        QScrollBar:horizontal {
            background: transparent;
            height: 8px;
            margin: 0;
        }
        QScrollBar::handle:horizontal {
            background: #2a2a52;
            border-radius: 4px;
            min-width: 30px;
        }
        QScrollBar::handle:horizontal:hover {
            background: #00d4ff;
        }
        QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal,
        QScrollBar::add-page:horizontal, QScrollBar::sub-page:horizontal {
            background: transparent;
            width: 0px;
        }
    )");

    QWidget* monitoringTab = new QWidget();
    monitoringTab->setStyleSheet("background: transparent;");
    QVBoxLayout* monitoringLayout = new QVBoxLayout(monitoringTab);

    QWidget* settingsTab = new QWidget();
    settingsTab->setStyleSheet("background: transparent;");
    QHBoxLayout* settingsColumnsLayout = new QHBoxLayout(settingsTab);
    settingsColumnsLayout->setSpacing(20);

    // Alfa Laval logo (SVG rendered in white) - for tab bar corner
    logoLabel = new QLabel(this);

    // Use SVG renderer for crisp vector graphics
    QSvgRenderer svgRenderer(QString("../alfa.svg"));
    QPixmap logoPixmap(85, 32);  // Original size for sharp rendering
    logoPixmap.fill(Qt::transparent);

    // Render SVG
    QPainter painter(&logoPixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    svgRenderer.render(&painter);
    painter.end();

    // Create white version using composition mode
    QPainter maskPainter(&logoPixmap);
    maskPainter.setCompositionMode(QPainter::CompositionMode_SourceIn);
    maskPainter.fillRect(logoPixmap.rect(), Qt::white);
    maskPainter.end();

    logoLabel->setPixmap(logoPixmap);
    logoLabel->setStyleSheet("padding: 8px; margin-right: 10px;");

    // Title
    QLabel* title = new QLabel("Real-time Monitoring", this);
    title->setAlignment(Qt::AlignCenter);
    title->setStyleSheet("font-size: 24px; font-weight: bold; padding: 10px; color: #00d4ff;");
    monitoringLayout->addWidget(title);

    // Status bar
    QHBoxLayout* statusLayout = new QHBoxLayout();

    statusLayout->addStretch();

    alarmBellLabel = new QLabel(this);
    alarmBellLabel->setPixmap(bellPixmap(QColor("#4ade80"), 30));
    statusLayout->addWidget(alarmBellLabel);

    alarmCountLabel = new QLabel("Alarms: 0", this);
    alarmCountLabel->setStyleSheet("font-size: 26px; font-weight: bold; color: #4ade80;");
    statusLayout->addWidget(alarmCountLabel);

    diagnoseButton = new QPushButton("Diagnose", this);
    diagnoseButton->setStyleSheet("background: #ff6b6b; color: white; font-weight: bold;");
    diagnoseButton->hide();  // Initially hidden
    connect(diagnoseButton, &QPushButton::clicked, this, &MainWindow::onDiagnoseClicked);
    statusLayout->addWidget(diagnoseButton);

    monitoringLayout->addLayout(statusLayout);

    // Three-column layout: Hot | Image | Cold
    QHBoxLayout* columnsLayout = new QHBoxLayout();
    columnsLayout->setSpacing(10);

    // Heat exchanger image (center)
    QLabel* imageLabel = new QLabel();
    QPixmap pixmap("../heatex.png");
    QTransform flip;
    flip.scale(1, -1);
    imageLabel->setPixmap(pixmap.transformed(flip).scaled(400, 500, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    imageLabel->setAlignment(Qt::AlignCenter);

    // Hot side (left column)
    QGroupBox* hotGroup = new QGroupBox("Hot Side", this);
    hotGroup->setStyleSheet(R"(
        QGroupBox {
            font-size: 16px;
            font-weight: bold;
            color: #ff6b6b;
            border: 2px solid #ff6b6b;
            border-radius: 10px;
            margin-top: 12px;
            padding: 8px;
        }
        QGroupBox::title {
            subcontrol-origin: margin;
            left: 15px;
            padding: 0 10px;
        }
    )");
    QVBoxLayout* hotLayout = new QVBoxLayout(hotGroup);
    hotLayout->setSpacing(8);

    // Cold side (right column)
    QGroupBox* coldGroup = new QGroupBox("Cold Side", this);
    coldGroup->setStyleSheet(R"(
        QGroupBox {
            font-size: 16px;
            font-weight: bold;
            color: #00d4ff;
            border: 2px solid #00d4ff;
            border-radius: 10px;
            margin-top: 12px;
            padding: 8px;
        }
        QGroupBox::title {
            subcontrol-origin: margin;
            left: 15px;
            padding: 0 10px;
        }
    )");
    QVBoxLayout* coldLayout = new QVBoxLayout(coldGroup);
    coldLayout->setSpacing(8);

    const auto& sensorConfigs = Config::instance().getSensors();
    for (const auto& sensor : sensorConfigs) {
        QLabel* label = new QLabel(QString::fromStdString(sensor.name + ": --- " + sensor.unit), this);
        label->setStyleSheet("font-size: 22px; padding: 5px;");
        label->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
        sensorLabels[sensor.id] = label;

        // Route to appropriate side
        if (sensor.id.find("hot") != std::string::npos) {
            hotLayout->addWidget(label);
        } else {
            coldLayout->addWidget(label);
        }
    }

    hotLayout->addStretch();
    coldLayout->addStretch();

    columnsLayout->addWidget(hotGroup, 1);
    columnsLayout->addWidget(imageLabel, 0);
    columnsLayout->addWidget(coldGroup, 1);

    monitoringLayout->addWidget(title);
    monitoringLayout->addLayout(statusLayout);
    monitoringLayout->addLayout(columnsLayout, 1);
    monitoringLayout->addStretch();

    tabWidget->addTab(monitoringTab, "Monitoring");

    // Graphs tab
    QWidget* graphsTab = new QWidget();
    graphsTab->setStyleSheet("background: transparent;");
    QHBoxLayout* graphsMainLayout = new QHBoxLayout(graphsTab);
    graphsMainLayout->setContentsMargins(10, 10, 10, 10);
    graphsMainLayout->setSpacing(12);

    // === LEFT PANEL: sensor checkboxes + time range ===
    QWidget* leftPanel = new QWidget();
    leftPanel->setFixedWidth(230);
    leftPanel->setStyleSheet("background: rgba(20, 20, 40, 0.75); border-radius: 8px;");
    QVBoxLayout* leftLayout = new QVBoxLayout(leftPanel);
    leftLayout->setContentsMargins(12, 14, 12, 14);
    leftLayout->setSpacing(8);

    QLabel* sensorsHeader = new QLabel("SENSORS");
    sensorsHeader->setStyleSheet(
        "color: #a0a0c0; font-size: 13px; font-weight: bold; letter-spacing: 2px;"
        " padding-bottom: 6px; border-bottom: 1px solid #2a2a52;");
    leftLayout->addWidget(sensorsHeader);

    // Scroll area for checkboxes
    QScrollArea* cbScroll = new QScrollArea();
    cbScroll->setWidgetResizable(true);
    cbScroll->setStyleSheet(
        "QScrollArea { background: transparent; border: none; }"
        "QScrollBar:vertical { width: 6px; background: #18183a; }"
        "QScrollBar::handle:vertical { background: #2a2a52; border-radius: 3px; }");
    QWidget* cbContainer = new QWidget();
    cbContainer->setStyleSheet("background: transparent;");
    QVBoxLayout* cbLayout = new QVBoxLayout(cbContainer);
    cbLayout->setContentsMargins(0, 4, 0, 4);
    cbLayout->setSpacing(6);

    static const std::vector<QColor> kSeriesColors = {
        QColor("#00d4ff"), QColor("#ff6b6b"), QColor("#51cf66"), QColor("#ffd43b"),
        QColor("#cc5de8"), QColor("#ff922b"), QColor("#74c0fc"), QColor("#f06595"),
    };

    int colorIdx = 0;
    auto makeCheckboxStyle = [](const QColor& c) -> QString {
        return QString(
            "QCheckBox { color: white; font-size: 15px; spacing: 8px; padding: 4px 2px; }"
            "QCheckBox::indicator { width: 18px; height: 18px; border-radius: 4px;"
            "  border: 2px solid %1; }"
            "QCheckBox::indicator:checked { background-color: %1; }"
            "QCheckBox::indicator:unchecked { background-color: transparent; }"
        ).arg(c.name());
    };

    // One checkbox per sensor
    for (const auto& sensor : sensorConfigs) {
        QColor color = kSeriesColors[colorIdx % kSeriesColors.size()];
        colorIdx++;

        QCheckBox* cb = new QCheckBox(QString::fromStdString(sensor.name));
        cb->setStyleSheet(makeCheckboxStyle(color));
        sensorCheckBoxes[sensor.id] = cb;

        QLineSeries* series = new QLineSeries(this);
        series->setColor(color);
        series->setPen(QPen(color, 2));
        series->setName(QString::fromStdString(sensor.name));
        chartSeriesMap[sensor.id] = series;

        cbLayout->addWidget(cb);

        connect(cb, &QCheckBox::toggled, this, [this, id = sensor.id](bool checked) {
            QLineSeries* s = chartSeriesMap[id];
            QChart* ch = chartView->chart();
            if (checked) {
                ch->addSeries(s);
                s->attachAxis(axisX);
                s->attachAxis(axisY);
            } else {
                ch->removeSeries(s);
                s->setParent(this);
            }
            updateChart();
            updateLegend();
        });
    }

    // BTUs checkbox
    {
        QColor color = kSeriesColors[colorIdx % kSeriesColors.size()];
        QCheckBox* cb = new QCheckBox("BTUs");
        cb->setStyleSheet(makeCheckboxStyle(color));
        sensorCheckBoxes["btus"] = cb;

        QLineSeries* series = new QLineSeries(this);
        series->setColor(color);
        series->setPen(QPen(color, 2));
        series->setName("BTUs");
        chartSeriesMap["btus"] = series;

        cbLayout->addWidget(cb);

        connect(cb, &QCheckBox::toggled, this, [this](bool checked) {
            QLineSeries* s = chartSeriesMap["btus"];
            QChart* ch = chartView->chart();
            if (checked) {
                ch->addSeries(s);
                s->attachAxis(axisX);
                s->attachAxis(axisY);
            } else {
                ch->removeSeries(s);
                s->setParent(this);
            }
            updateChart();
            updateLegend();
        });
    }

    cbLayout->addStretch();
    cbScroll->setWidget(cbContainer);
    leftLayout->addWidget(cbScroll, 1);


    // Time range section
    QLabel* timeHeader = new QLabel("TIME RANGE");
    timeHeader->setStyleSheet(
        "color: #a0a0c0; font-size: 13px; font-weight: bold; letter-spacing: 2px;"
        " padding-top: 8px; border-top: 1px solid #2a2a52; margin-top: 4px;");
    leftLayout->addWidget(timeHeader);

    QString comboStyle = R"(
        QComboBox {
            font-size: 16px;
            padding: 8px 10px;
            min-height: 30px;
            background: #18183a;
            color: white;
            border: 2px solid #2a2a52;
            border-radius: 6px;
        }
        QComboBox::drop-down { width: 30px; }
        QComboBox QAbstractItemView {
            font-size: 20px;
            background: #18183a;
            color: white;
            outline: none;
        }
        QComboBox QAbstractItemView::item {
            min-height: 50px;
            padding: 10px 8px;
            border-bottom: 1px solid #2a2a52;
        }
        QComboBox QAbstractItemView::item:selected {
            background-color: #2a2a52;
            color: white;
        }
        QComboBox QAbstractItemView::item:hover {
            background-color: #21213e;
        }
    )";

    timeRangeComboBox = new QComboBox(this);
    timeRangeComboBox->setStyleSheet(comboStyle);
    timeRangeComboBox->addItem("30 seconds", 30);
    timeRangeComboBox->addItem("1 minute", 60);
    timeRangeComboBox->addItem("2 minutes", 120);
    timeRangeComboBox->addItem("5 minutes", 300);
    timeRangeComboBox->setCurrentIndex(1);
    selectedTimeRange = 60;
    connect(timeRangeComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MainWindow::onTimeRangeSelected);
    leftLayout->addWidget(timeRangeComboBox);

    graphsMainLayout->addWidget(leftPanel);

    // === RIGHT PANEL: chart + legend ===
    QVBoxLayout* rightLayout = new QVBoxLayout();
    rightLayout->setSpacing(0);
    rightLayout->setContentsMargins(0, 0, 0, 0);

    // Chart (no series yet — added dynamically by checkboxes)
    QChart* chart = new QChart();
    chart->legend()->hide();
    chart->setBackgroundBrush(QBrush(QColor("#141428")));
    chart->setPlotAreaBackgroundBrush(QBrush(QColor("#18183a")));
    chart->setPlotAreaBackgroundVisible(true);
    chart->setMargins(QMargins(10, 10, 10, 10));

    axisX = new QValueAxis();
    axisX->setTitleText("Time (seconds)");
    axisX->setRange(0, 60);
    axisX->setLabelFormat("%d");
    axisX->setTitleBrush(QBrush(QColor("#a0a0b0")));
    axisX->setLabelsBrush(QBrush(QColor("#a0a0b0")));
    axisX->setLinePenColor(QColor("#2a2a52"));
    axisX->setGridLinePen(QPen(QColor("#22224a"), 1));
    chart->addAxis(axisX, Qt::AlignBottom);

    axisY = new QValueAxis();
    axisY->setTitleText("Value");
    axisY->setRange(50, 210);
    axisY->setTitleBrush(QBrush(QColor("#a0a0b0")));
    axisY->setLabelsBrush(QBrush(QColor("#a0a0b0")));
    axisY->setLinePenColor(QColor("#2a2a52"));
    axisY->setGridLinePen(QPen(QColor("#22224a"), 1));
    chart->addAxis(axisY, Qt::AlignLeft);

    chartView = new QChartView(chart);
    chartView->setRenderHint(QPainter::Antialiasing);
    chartView->setStyleSheet("background: #141428; border: none;");
    rightLayout->addWidget(chartView, 1);

    // Legend widget at the bottom
    legendWidget = new QWidget();
    legendWidget->setFixedHeight(48);
    legendWidget->setStyleSheet(
        "background: rgba(20, 20, 40, 0.85); border-top: 1px solid #2a2a52;");
    QHBoxLayout* legendLayout = new QHBoxLayout(legendWidget);
    legendLayout->setContentsMargins(14, 8, 14, 8);
    legendLayout->setSpacing(20);
    QLabel* legendEmpty = new QLabel("Select sensors to display");
    legendEmpty->setStyleSheet("color: #606080; font-size: 14px;");
    legendLayout->addWidget(legendEmpty);
    legendLayout->addStretch();
    rightLayout->addWidget(legendWidget);

    graphsMainLayout->addLayout(rightLayout, 1);

    // Default: check all sensor checkboxes (except BTUs) — done after chart/axes/legend are ready
    for (const auto& sensor : sensorConfigs) {
        sensorCheckBoxes[sensor.id]->setChecked(true);
    }

    tabWidget->addTab(graphsTab, "Graphs");

    // IOM tab (PDF viewer for installation/operation manual)
    QWidget* iomTab = new QWidget();
    iomTab->setStyleSheet("background: transparent;");
    QVBoxLayout* iomLayout = new QVBoxLayout(iomTab);
    iomLayout->setContentsMargins(0, 0, 0, 0);

    pdfScene = new QGraphicsScene(this);
    pdfView = new QGraphicsView(pdfScene, this);
    pdfView->setRenderHint(QPainter::Antialiasing);
    pdfView->setRenderHint(QPainter::SmoothPixmapTransform);
    pdfView->setStyleSheet("QGraphicsView { border: none; background: #1e1e40; }");
    pdfView->grabGesture(Qt::PinchGesture);
    pdfView->viewport()->grabGesture(Qt::PinchGesture);
    pdfView->viewport()->setAttribute(Qt::WA_AcceptTouchEvents);
    pdfView->setAttribute(Qt::WA_AcceptTouchEvents);
    pdfView->installEventFilter(this);
    pdfView->viewport()->installEventFilter(this);
    QScroller::grabGesture(pdfView->viewport(), QScroller::TouchGesture);

    pdfDocument = Poppler::Document::load(QString("../iom.pdf"));
    if (pdfDocument && !pdfDocument->isLocked()) {
        pdfDocument->setRenderHint(Poppler::Document::TextAntialiasing);
        pdfDocument->setRenderHint(Poppler::Document::Antialiasing);

        int availableWidth = displayConfig.width - 40;
        double yOffset = 0;

        for (int i = 0; i < pdfDocument->numPages(); i++) {
            std::unique_ptr<Poppler::Page> page = pdfDocument->page(i);
            if (page) {
                QSizeF pageSize = page->pageSizeF();
                double dpi = (availableWidth / pageSize.width()) * 72.0;

                QImage image = page->renderToImage(dpi, dpi);
                QGraphicsPixmapItem* item = pdfScene->addPixmap(QPixmap::fromImage(image));
                item->setPos(0, yOffset);
                yOffset += image.height() + 10;
            }
        }
    } else {
        QGraphicsTextItem* errorText = pdfScene->addText("Could not load IOM PDF.\nPlace iom.pdf in the application directory.");
        errorText->setDefaultTextColor(QColor("#ff6b6b"));
        errorText->setFont(QFont("sans-serif", 16));
    }

    iomLayout->addWidget(pdfView);

    tabWidget->addTab(iomTab, "IOM");

    // Parts List tab
    QWidget* partsTab = new QWidget();
    partsTab->setStyleSheet("background: transparent;");
    QHBoxLayout* partsColumnsLayout = new QHBoxLayout(partsTab);
    partsColumnsLayout->setSpacing(20);

    // Left column: Order form
    QGroupBox* orderGroup = new QGroupBox("Order Parts", this);
    orderGroup->setStyleSheet(R"(
        QGroupBox {
            font-size: 20px;
            font-weight: bold;
            color: white;
            border: 2px solid white;
            border-radius: 10px;
            margin-top: 20px;
            padding: 15px;
        }
        QGroupBox::title {
            subcontrol-origin: margin;
            left: 15px;
            padding: 0 10px;
        }
    )");
    QVBoxLayout* orderLayout = new QVBoxLayout(orderGroup);
    orderLayout->setSpacing(12);

    QString inputStyle = R"(
        QLineEdit {
            background: #18183a;
            border: 2px solid #2a2a52;
            border-radius: 6px;
            color: white;
            padding: 10px;
            font-size: 16px;
        }
        QLineEdit:focus {
            border-color: #00d4ff;
        }
    )";

    QLabel* partLabel = new QLabel("Part Number:", this);
    partLabel->setStyleSheet("font-size: 16px; color: white;");
    orderLayout->addWidget(partLabel);

    orderPartNumber = new QLineEdit(this);
    orderPartNumber->setPlaceholderText("Select an item from the table");
    orderPartNumber->setStyleSheet(inputStyle);
    orderLayout->addWidget(orderPartNumber);

    QLabel* nameLabel = new QLabel("Part Name:", this);
    nameLabel->setStyleSheet("font-size: 16px; color: white;");
    orderLayout->addWidget(nameLabel);

    orderPartName = new QLineEdit(this);
    orderPartName->setPlaceholderText("Select an item from the table");
    orderPartName->setStyleSheet(inputStyle);
    orderLayout->addWidget(orderPartName);

    QLabel* qtyLabel = new QLabel("Quantity:", this);
    qtyLabel->setStyleSheet("font-size: 16px; color: white;");
    orderLayout->addWidget(qtyLabel);

    orderQuantity = new QLineEdit(this);
    orderQuantity->setPlaceholderText("Select an item from the table");
    orderQuantity->setStyleSheet(inputStyle);
    orderLayout->addWidget(orderQuantity);

    QPushButton* generateBtn = new QPushButton("Add to Order", this);
    generateBtn->setStyleSheet(R"(
        QPushButton {
            background: #1e2044;
            color: white;
            font-size: 18px;
            font-weight: bold;
            padding: 12px;
            border-radius: 6px;
            border: 2px solid #00d4ff;
        }
        QPushButton:pressed {
            background: #141428;
        }
    )");
    connect(generateBtn, &QPushButton::clicked, this, &MainWindow::onGenerateOrder);
    orderLayout->addWidget(generateBtn);

    orderLayout->addStretch();

    orderQrLabel = new QLabel(this);
    orderQrLabel->setAlignment(Qt::AlignCenter);
    orderLayout->addWidget(orderQrLabel);

    orderQrHint = new QLabel(this);
    orderQrHint->setText("Scan to complete your order");
    orderQrHint->setAlignment(Qt::AlignCenter);
    orderQrHint->setStyleSheet("font-size: 16px; color: white; padding: 5px;");
    orderQrHint->hide();
    orderLayout->addWidget(orderQrHint);

    orderLayout->addStretch();

    QString tableStyle = R"(
        QTableWidget {
            background-color: #141428;
            alternate-background-color: #18183a;
            gridline-color: #22224a;
            font-size: 16px;
        }
        QTableWidget::item {
            padding: 10px 8px;
        }
        QTableWidget::item:selected {
            background-color: #1e2044;
        }
        QHeaderView::section {
            background-color: #1e2044;
            color: white;
            padding: 10px;
            border: 1px solid #141428;
            font-weight: bold;
            font-size: 15px;
        }
    )";

    // Right column: split into parts catalog (top) and current order (bottom)
    QWidget* tableContainer = new QWidget();
    QVBoxLayout* tableLayout = new QVBoxLayout(tableContainer);
    tableLayout->setContentsMargins(0, 0, 0, 8);
    tableLayout->setSpacing(6);

    // --- Parts catalog ---
    QLabel* catalogLabel = new QLabel("Parts Catalog", this);
    catalogLabel->setStyleSheet("font-size: 16px; font-weight: bold; color: white; padding: 4px 0;");
    tableLayout->addWidget(catalogLabel);

    partsTable = new QTableWidget(this);
    partsTable->setColumnCount(3);
    partsTable->setHorizontalHeaderLabels({"Item No", "Item Description", "Quantity"});
    partsTable->verticalHeader()->setVisible(false);
    partsTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Fixed);
    partsTable->setColumnWidth(0, 240);
    partsTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    partsTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    partsTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    partsTable->setSelectionMode(QAbstractItemView::SingleSelection);
    partsTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    partsTable->setAlternatingRowColors(true);
    partsTable->setStyleSheet(tableStyle);

    QFile csvFile("../parts_list.csv");
    if (csvFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream in(&csvFile);
        bool firstLine = true;
        while (!in.atEnd()) {
            QString line = in.readLine();
            if (firstLine) { firstLine = false; continue; }
            QStringList fields = line.split(',');
            if (fields.size() >= 5) {
                int row = partsTable->rowCount();
                partsTable->insertRow(row);
                auto* itemNo = new QTableWidgetItem(fields[2].trimmed());
                itemNo->setTextAlignment(Qt::AlignCenter);
                partsTable->setItem(row, 0, itemNo);
                auto* itemDesc = new QTableWidgetItem(fields[3].trimmed());
                itemDesc->setTextAlignment(Qt::AlignCenter);
                partsTable->setItem(row, 1, itemDesc);
                auto* itemQty = new QTableWidgetItem(fields[4].trimmed());
                itemQty->setTextAlignment(Qt::AlignCenter);
                partsTable->setItem(row, 2, itemQty);
                partsTable->setRowHeight(row, 44);
            }
        }
        csvFile.close();
    }

    connect(partsTable, &QTableWidget::cellPressed, this, [this](int row, int) {
        partsTable->selectRow(row);
        if (auto* i = partsTable->item(row, 0)) orderPartNumber->setText(i->text());
        if (auto* i = partsTable->item(row, 1)) orderPartName->setText(i->text());
        if (auto* i = partsTable->item(row, 2)) orderQuantity->setText(i->text());
        QTimer::singleShot(150, this, [this]() { partsTable->clearSelection(); });
    });

    QScroller::grabGesture(partsTable->viewport(), QScroller::TouchGesture);
    tableLayout->addWidget(partsTable, 3);

    // --- Current order ---
    QWidget* orderHeaderRow = new QWidget();
    QHBoxLayout* orderHeaderLayout = new QHBoxLayout(orderHeaderRow);
    orderHeaderLayout->setContentsMargins(0, 4, 0, 0);
    orderHeaderLayout->setSpacing(10);

    QLabel* orderTableLabel = new QLabel("Current Order", this);
    orderTableLabel->setStyleSheet("font-size: 16px; font-weight: bold; color: white;");
    orderHeaderLayout->addWidget(orderTableLabel);
    orderHeaderLayout->addStretch();

    QPushButton* clearOrderBtn = new QPushButton("Clear", this);
    clearOrderBtn->setStyleSheet(R"(
        QPushButton {
            background: #1e2044; color: #ff6b6b;
            font-size: 13px; font-weight: bold;
            padding: 4px 12px; border-radius: 4px;
            border: 1px solid #ff6b6b;
        }
        QPushButton:pressed { background: #141428; }
    )");
    orderHeaderLayout->addWidget(clearOrderBtn);
    tableLayout->addWidget(orderHeaderRow);

    orderTable = new QTableWidget(this);
    orderTable->setColumnCount(3);
    orderTable->setHorizontalHeaderLabels({"Item No", "Item Description", "Quantity"});
    orderTable->verticalHeader()->setVisible(false);
    orderTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Fixed);
    orderTable->setColumnWidth(0, 240);
    orderTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    orderTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    orderTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    orderTable->setSelectionMode(QAbstractItemView::SingleSelection);
    orderTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    orderTable->setAlternatingRowColors(true);
    orderTable->setStyleSheet(tableStyle);
    QScroller::grabGesture(orderTable->viewport(), QScroller::TouchGesture);
    tableLayout->addWidget(orderTable, 2);

    connect(clearOrderBtn, &QPushButton::clicked, this, [this]() {
        orderTable->setRowCount(0);
        orderQrLabel->clear();
        orderQrHint->hide();
    });

    partsColumnsLayout->addWidget(orderGroup, 1);
    partsColumnsLayout->addWidget(tableContainer, 2);
    tabWidget->addTab(partsTab, "Parts List");

    // Videos tab
    QWidget* videosTab = new QWidget();
    videosTab->setStyleSheet("background: transparent;");
    QHBoxLayout* videosLayout = new QHBoxLayout(videosTab);
    videosLayout->setSpacing(0);
    videosLayout->setContentsMargins(0, 0, 0, 0);

    // Left panel - video list
    videoList = new QListWidget(this);
    videoList->setFixedWidth(260);
    videoList->setStyleSheet(R"(
        QListWidget {
            background: #18183a;
            border: none;
            font-size: 18px;
            color: white;
        }
        QListWidget::item {
            padding: 18px 15px;
            border-bottom: 1px solid #1e1e40;
        }
        QListWidget::item:selected {
            background: #00d4ff;
            color: #141428;
        }
        QListWidget::item:hover:!selected {
            background: #21213e;
        }
    )");

    const QString videoDir = QCoreApplication::applicationDirPath();
    struct VideoEntry { QString title; QString file; };
    const QList<VideoEntry> videos = {
        { "How it Works",          "" },
        { "Exploded Parts View",   videoDir + "/exploded.mp4" },
        { "Opening Heat Exchanger","" },
        { "Cleaning",              "" },
        { "Changing Gaskets",      "" },
        { "Closing Heat Exchanger","" },
    };
    for (const auto& v : videos) {
        QListWidgetItem* item = new QListWidgetItem(v.title, videoList);
        item->setData(Qt::UserRole, v.file);
        if (v.file.isEmpty()) {
            item->setForeground(QColor("#666"));
        }
    }

    // Right panel
    QWidget* videoRight = new QWidget(this);
    QVBoxLayout* videoRightLayout = new QVBoxLayout(videoRight);
    videoRightLayout->setContentsMargins(0, 0, 0, 0);
    videoRightLayout->setSpacing(0);

    videoPlayer = new QMediaPlayer(this);
    audioOutput = new QAudioOutput(this);
    videoPlayer->setAudioOutput(audioOutput);

    // "No video" placeholder
    videoUnavailableLabel = new QLabel("Select a video from the list", this);
    videoUnavailableLabel->setAlignment(Qt::AlignCenter);
    videoUnavailableLabel->setStyleSheet("font-size: 20px; color: #aaa; background: transparent;");

    // Video container with overlay using StackAll
    videoContainer = new QWidget(this);
    QStackedLayout* videoStack = new QStackedLayout(videoContainer);
    videoStack->setStackingMode(QStackedLayout::StackAll);
    videoStack->setContentsMargins(0, 0, 0, 0);

    videoWidget = new QVideoWidget();
    videoWidget->setStyleSheet("background: black;");
    videoPlayer->setVideoOutput(videoWidget);

    // Transparent click-to-play overlay (no children needed)
    videoClickArea = new QWidget();
    videoClickArea->setStyleSheet("background: transparent;");
    videoClickArea->installEventFilter(this);

    // Controls bar lives below the video so QVideoWidget can't paint over it
    videoControlsBar = new QWidget();
    videoControlsBar->setStyleSheet("background: #141428;");
    QVBoxLayout* controlsBarLayout = new QVBoxLayout(videoControlsBar);
    controlsBarLayout->setContentsMargins(15, 8, 15, 10);
    controlsBarLayout->setSpacing(6);

    // Seek slider
    videoSeekSlider = new QSlider(Qt::Horizontal);
    videoSeekSlider->setStyleSheet(R"(
        QSlider::groove:horizontal {
            height: 6px;
            background: #444;
            border-radius: 3px;
        }
        QSlider::sub-page:horizontal {
            background: #00d4ff;
            border-radius: 3px;
        }
        QSlider::handle:horizontal {
            width: 22px;
            height: 22px;
            margin: -8px 0;
            border-radius: 11px;
            background: white;
        }
    )");
    controlsBarLayout->addWidget(videoSeekSlider);

    // Play/pause button + time label
    QHBoxLayout* controlsRow = new QHBoxLayout();
    controlsRow->setSpacing(15);

    videoPauseButton = new QPushButton("▶", this);
    videoPauseButton->setStyleSheet(R"(
        QPushButton {
            font-size: 22px;
            color: white;
            background: transparent;
            border: none;
            padding: 0 10px;
        }
        QPushButton:pressed { color: #00d4ff; }
    )");
    videoPauseButton->setFixedWidth(50);

    videoTimeLabel = new QLabel("0:00 / 0:00", this);
    videoTimeLabel->setStyleSheet("font-size: 16px; color: #ccc;");

    controlsRow->addWidget(videoPauseButton);
    controlsRow->addWidget(videoTimeLabel);
    controlsRow->addStretch();
    controlsBarLayout->addLayout(controlsRow);

    videoStack->addWidget(videoWidget);
    videoStack->addWidget(videoClickArea);

    videoRightLayout->addWidget(videoUnavailableLabel);
    videoRightLayout->addWidget(videoContainer, 1);
    videoRightLayout->addWidget(videoControlsBar);
    videoContainer->hide();
    videoControlsBar->hide();

    // Format ms as m:ss
    auto formatTime = [](qint64 ms) -> QString {
        qint64 s = ms / 1000;
        return QString("%1:%2").arg(s / 60).arg(s % 60, 2, 10, QChar('0'));
    };

    connect(videoPlayer, &QMediaPlayer::positionChanged, this, [this, formatTime](qint64 pos) {
        if (!videoSeekSlider->isSliderDown())
            videoSeekSlider->setValue(pos);
        videoTimeLabel->setText(formatTime(pos) + " / " + formatTime(videoPlayer->duration()));
    });

    connect(videoPlayer, &QMediaPlayer::durationChanged, this, [this](qint64 duration) {
        videoSeekSlider->setRange(0, duration);
    });

    connect(videoSeekSlider, &QSlider::sliderMoved, videoPlayer, &QMediaPlayer::setPosition);

    connect(videoPauseButton, &QPushButton::clicked, this, [this]() {
        if (videoPlayer->playbackState() == QMediaPlayer::PlayingState) {
            videoPlayer->pause();
        } else {
            videoPlayer->play();
        }
    });

    connect(videoPlayer, &QMediaPlayer::playbackStateChanged, this, [this](QMediaPlayer::PlaybackState state) {
        videoPauseButton->setText(state == QMediaPlayer::PlayingState ? "⏸" : "▶");
    });
    connect(videoPlayer, &QMediaPlayer::errorOccurred, this,
            [this](QMediaPlayer::Error, const QString& errorString) {
        videoContainer->hide();
        videoControlsBar->hide();
        videoUnavailableLabel->setText("Playback error: " + errorString);
        videoUnavailableLabel->show();
    });

    videosLayout->addWidget(videoList);
    videosLayout->addWidget(videoRight, 1);

    connect(videoList, &QListWidget::itemClicked, this, &MainWindow::onVideoSelected);

    tabWidget->addTab(videosTab, "Videos");

    // Support tab
    QWidget* supportTab = new QWidget();
    supportTab->setStyleSheet("background: transparent;");
    QVBoxLayout* supportLayout = new QVBoxLayout(supportTab);
    supportLayout->setAlignment(Qt::AlignCenter);
    supportLayout->setSpacing(20);

    QLabel* supportTitle = new QLabel("Need help? Scan the QR code below to visit our support page.", this);
    supportTitle->setAlignment(Qt::AlignCenter);
    supportTitle->setStyleSheet("font-size: 20px; color: white;");
    supportTitle->setWordWrap(true);
    supportLayout->addWidget(supportTitle);

    QLabel* supportSubtitle = new QLabel("Your product details will be automatically submitted with your support request.", this);
    supportSubtitle->setAlignment(Qt::AlignCenter);
    supportSubtitle->setStyleSheet("font-size: 16px; color: #aaa;");
    supportSubtitle->setWordWrap(true);
    supportLayout->addWidget(supportSubtitle);

    QString supportUrl = "https://www.srs-enterprises.com/support";
    QRcode* supportQr = QRcode_encodeString(supportUrl.toUtf8().constData(), 0, QR_ECLEVEL_M, QR_MODE_8, 1);
    if (supportQr) {
        int scale = 10;
        int qrSize = supportQr->width * scale;
        QImage qrImage(qrSize, qrSize, QImage::Format_RGB32);
        qrImage.fill(Qt::white);
        for (int y = 0; y < supportQr->width; y++) {
            for (int x = 0; x < supportQr->width; x++) {
                if (supportQr->data[y * supportQr->width + x] & 1) {
                    for (int sy = 0; sy < scale; sy++) {
                        for (int sx = 0; sx < scale; sx++) {
                            qrImage.setPixel(x * scale + sx, y * scale + sy, qRgb(0, 0, 0));
                        }
                    }
                }
            }
        }
        QRcode_free(supportQr);

        QLabel* supportQrLabel = new QLabel(this);
        supportQrLabel->setPixmap(QPixmap::fromImage(qrImage));
        supportQrLabel->setAlignment(Qt::AlignCenter);
        supportLayout->addWidget(supportQrLabel);
    }

    tabWidget->addTab(supportTab, "Support");

    // === Left Column: Alarm Thresholds ===
    QGroupBox* thresholdGroup = new QGroupBox("Alarm Thresholds", this);
    thresholdGroup->setStyleSheet(R"(
        QGroupBox {
            font-size: 20px;
            font-weight: bold;
            color: #00d4ff;
            border: 2px solid #00d4ff;
            border-radius: 10px;
            margin-top: 20px;
            padding: 15px;
        }
        QGroupBox::title {
            subcontrol-origin: margin;
            left: 15px;
            padding: 0 10px;
        }
    )");
    QVBoxLayout* thresholdOuterLayout = new QVBoxLayout(thresholdGroup);

    QScrollArea* thresholdScroll = new QScrollArea(this);
    thresholdScroll->setWidgetResizable(true);
    thresholdScroll->setStyleSheet("QScrollArea { border: none; background: transparent; }");
    QScroller::grabGesture(thresholdScroll->viewport(), QScroller::TouchGesture);

    QWidget* thresholdContainer = new QWidget();
    QVBoxLayout* thresholdLayout = new QVBoxLayout(thresholdContainer);
    thresholdLayout->setSpacing(12);

    QString spinBoxStyle = R"(
        QDoubleSpinBox {
            background: #18183a;
            border: 2px solid #2a2a52;
            border-radius: 6px;
            color: white;
            padding: 4px;
            font-size: 18px;
            min-height: 32px;
            qproperty-alignment: AlignCenter;
        }
        QDoubleSpinBox::up-button, QDoubleSpinBox::down-button {
            width: 0px;
            height: 0px;
            border: none;
        }
    )";

    QString incDecBtnStyle = R"(
        QPushButton {
            background: #1e2044;
            color: #00d4ff;
            font-size: 24px;
            font-weight: bold;
            border: 2px solid #2a2a52;
            border-radius: 6px;
            min-width: 36px;
            max-width: 36px;
            min-height: 44px;
            padding: 0px;
        }
        QPushButton:pressed {
            background: #141428;
            border-color: #00d4ff;
        }
    )";

    for (const auto& sensor : Config::instance().getSensors()) {
        QLabel* sensorLabel = new QLabel(QString::fromStdString(sensor.name), this);
        sensorLabel->setStyleSheet("font-size: 16px; font-weight: bold; color: #e0e0e0; padding-top: 6px;");
        thresholdLayout->addWidget(sensorLabel);

        QHBoxLayout* rangeRow = new QHBoxLayout();
        rangeRow->setSpacing(8);

        // Low threshold
        QLabel* lowLabel = new QLabel("Low:", this);
        lowLabel->setStyleSheet("font-size: 14px; color: #aaa;");
        rangeRow->addWidget(lowLabel);

        QPushButton* lowMinus = new QPushButton("-", this);
        lowMinus->setStyleSheet(incDecBtnStyle);
        rangeRow->addWidget(lowMinus);

        QDoubleSpinBox* lowSpin = new QDoubleSpinBox(this);
        lowSpin->setRange(sensor.min, sensor.max);
        lowSpin->setDecimals(1);
        lowSpin->setSingleStep(0.5);
        lowSpin->setValue(alarmManager.getLowThreshold(sensor.id));
        lowSpin->setButtonSymbols(QAbstractSpinBox::NoButtons);
        lowSpin->setStyleSheet(spinBoxStyle);
        lowSpin->setReadOnly(true);
        rangeRow->addWidget(lowSpin);
        thresholdLowEdits[sensor.id] = lowSpin;

        QPushButton* lowPlus = new QPushButton("+", this);
        lowPlus->setStyleSheet(incDecBtnStyle);
        rangeRow->addWidget(lowPlus);

        connect(lowMinus, &QPushButton::clicked, [lowSpin]() { lowSpin->stepDown(); });
        connect(lowPlus, &QPushButton::clicked, [lowSpin]() { lowSpin->stepUp(); });

        rangeRow->addSpacing(16);

        // High threshold
        QLabel* highLabel = new QLabel("High:", this);
        highLabel->setStyleSheet("font-size: 14px; color: #aaa;");
        rangeRow->addWidget(highLabel);

        QPushButton* highMinus = new QPushButton("-", this);
        highMinus->setStyleSheet(incDecBtnStyle);
        rangeRow->addWidget(highMinus);

        QDoubleSpinBox* highSpin = new QDoubleSpinBox(this);
        highSpin->setRange(sensor.min, sensor.max);
        highSpin->setDecimals(1);
        highSpin->setSingleStep(0.5);
        highSpin->setValue(alarmManager.getHighThreshold(sensor.id));
        highSpin->setButtonSymbols(QAbstractSpinBox::NoButtons);
        highSpin->setStyleSheet(spinBoxStyle);
        highSpin->setReadOnly(true);
        rangeRow->addWidget(highSpin);
        thresholdHighEdits[sensor.id] = highSpin;

        QPushButton* highPlus = new QPushButton("+", this);
        highPlus->setStyleSheet(incDecBtnStyle);
        rangeRow->addWidget(highPlus);

        connect(highMinus, &QPushButton::clicked, [highSpin]() { highSpin->stepDown(); });
        connect(highPlus, &QPushButton::clicked, [highSpin]() { highSpin->stepUp(); });

        thresholdLayout->addLayout(rangeRow);
    }

    thresholdLayout->addStretch();
    thresholdScroll->setWidget(thresholdContainer);
    thresholdOuterLayout->addWidget(thresholdScroll);

    QPushButton* saveThresholdsBtn = new QPushButton("Save", this);
    saveThresholdsBtn->setStyleSheet(R"(
        QPushButton {
            background: #1e2044;
            color: #00d4ff;
            font-size: 18px;
            font-weight: bold;
            padding: 12px;
            border-radius: 6px;
            border: 2px solid #00d4ff;
        }
        QPushButton:pressed {
            background: #141428;
        }
    )");
    connect(saveThresholdsBtn, &QPushButton::clicked, this, &MainWindow::onSaveThresholds);
    thresholdOuterLayout->addWidget(saveThresholdsBtn);

    settingsColumnsLayout->addWidget(thresholdGroup, 1);

    // === Right Column: About Section ===
    QGroupBox* aboutGroup = new QGroupBox("About", this);
    aboutGroup->setStyleSheet(R"(
        QGroupBox {
            font-size: 20px;
            font-weight: bold;
            color: #00d4ff;
            border: 2px solid #00d4ff;
            border-radius: 10px;
            margin-top: 20px;
            padding: 15px;
        }
        QGroupBox::title {
            subcontrol-origin: margin;
            left: 15px;
            padding: 0 10px;
        }
    )");
    QVBoxLayout* aboutLayout = new QVBoxLayout(aboutGroup);
    aboutLayout->setSpacing(10);

    const auto& bacnetConfig = Config::instance().getBACnetConfig();

    auto addInfoRow = [&](const QString& label, const QString& value) {
        QHBoxLayout* row = new QHBoxLayout();
        QLabel* lbl = new QLabel(label, this);
        lbl->setStyleSheet("font-size: 16px; color: #aaa;");
        QLabel* val = new QLabel(value, this);
        val->setStyleSheet("font-size: 16px; color: #e0e0e0; font-weight: bold;");
        row->addWidget(lbl);
        row->addStretch();
        row->addWidget(val);
        aboutLayout->addLayout(row);
    };

    addInfoRow("Device Name:", QString::fromStdString(bacnetConfig.device_name));
    addInfoRow("Device ID:", QString::number(bacnetConfig.device_id));
    addInfoRow("Vendor ID:", QString::number(bacnetConfig.vendor_id));
    addInfoRow("IP Address:", QString::fromStdString(bacnetConfig.ip_address));
    addInfoRow("Port:", QString::number(bacnetConfig.ip_port));

    aboutLayout->addSpacing(20);

    // QR code for website
    QString aboutUrl = "https://www.srs-enterprises.com";
    QRcode* aboutQr = QRcode_encodeString(aboutUrl.toUtf8().constData(), 0, QR_ECLEVEL_M, QR_MODE_8, 1);
    if (aboutQr) {
        int scale = 10;
        int qrSize = aboutQr->width * scale;
        QImage qrImage(qrSize, qrSize, QImage::Format_RGB32);
        qrImage.fill(Qt::white);

        for (int y = 0; y < aboutQr->width; y++) {
            for (int x = 0; x < aboutQr->width; x++) {
                if (aboutQr->data[y * aboutQr->width + x] & 1) {
                    for (int sy = 0; sy < scale; sy++) {
                        for (int sx = 0; sx < scale; sx++) {
                            qrImage.setPixel(x * scale + sx, y * scale + sy, qRgb(0, 0, 0));
                        }
                    }
                }
            }
        }
        QRcode_free(aboutQr);

        QLabel* aboutQrLabel = new QLabel(this);
        aboutQrLabel->setPixmap(QPixmap::fromImage(qrImage));
        aboutQrLabel->setAlignment(Qt::AlignCenter);
        aboutLayout->addWidget(aboutQrLabel);

        QLabel* aboutQrHint = new QLabel("Scan for more details", this);
        aboutQrHint->setStyleSheet("font-size: 16px; color: white;");
        aboutQrHint->setAlignment(Qt::AlignCenter);
        aboutLayout->addWidget(aboutQrHint);
    }

    aboutLayout->addStretch();
    settingsColumnsLayout->addWidget(aboutGroup, 1);

    tabWidget->addTab(settingsTab, "Settings");

    // Dev tab (conditional)
    if (displayConfig.devMode) {
        QWidget* devTab = new QWidget();
        QVBoxLayout* devLayout = new QVBoxLayout(devTab);

        QLabel* devTitle = new QLabel("Alarm Testing", this);
        devTitle->setStyleSheet("font-size: 18px; font-weight: bold; padding: 10px;");
        devLayout->addWidget(devTitle);

        // Create buttons for each sensor alarm
        const auto& sensorConfigs = Config::instance().getSensors();
        for (const auto& sensor : sensorConfigs) {
            QHBoxLayout* rowLayout = new QHBoxLayout();

            QPushButton* highBtn = new QPushButton(QString::fromStdString(sensor.name + " HIGH"), this);
            highBtn->setProperty("sensorId", QString::fromStdString(sensor.id));
            highBtn->setProperty("alarmType", "HIGH");
            connect(highBtn, &QPushButton::clicked, this, &MainWindow::onTriggerAlarm);
            rowLayout->addWidget(highBtn);

            QPushButton* lowBtn = new QPushButton(QString::fromStdString(sensor.name + " LOW"), this);
            lowBtn->setProperty("sensorId", QString::fromStdString(sensor.id));
            lowBtn->setProperty("alarmType", "LOW");
            connect(lowBtn, &QPushButton::clicked, this, &MainWindow::onTriggerAlarm);
            rowLayout->addWidget(lowBtn);

            devLayout->addLayout(rowLayout);
        }

        // Reset all button
        QPushButton* resetBtn = new QPushButton("Reset All Alarms", this);
        resetBtn->setStyleSheet("background-color: #4CAF50; color: white; font-weight: bold; padding: 10px;");
        connect(resetBtn, &QPushButton::clicked, this, &MainWindow::onResetAlarms);
        devLayout->addWidget(resetBtn);

        devLayout->addStretch();
        tabWidget->addTab(devTab, "Dev");
    }

    // Add logo to top-right corner of tab bar
    tabWidget->setCornerWidget(logoLabel, Qt::TopRightCorner);

    mainLayout->addWidget(tabWidget);
    //mainLayout->addLayout(sensorGrid);
    //mainLayout->addStretch();

    // Initial update
    updateDisplay();
}

void MainWindow::onUpdateTimer() {
    // Update sensor values
    sensorManager.update();

    // Check alarms
    alarmManager.checkAlarms(sensorManager.getAllValues());

    // Update BACnet object values
    bacnetInterface.updateValues(sensorManager, alarmManager);

    // Update display
    updateDisplay();
}

void MainWindow::onBACnetTimer() {
    // Process BACnet messages
    bacnetInterface.process();
}

void MainWindow::updateDisplay() {
    auto values = sensorManager.getAllValues();
    const auto& sensorConfigs = Config::instance().getSensors();

    // Update sensor labels
    for (const auto& sensor : sensorConfigs) {
        if (sensorLabels.count(sensor.id)) {
            float value = values[sensor.id];
            QString text = QString::fromStdString(sensor.name) +
                          QString(": %1 ").arg(value, 0, 'f', 1) +
                          QString::fromStdString(sensor.unit);

            // Color based on alarm state
            bool inAlarm = alarmManager.isAlarmActive(sensor.id, "HIGH") ||
                           alarmManager.isAlarmActive(sensor.id, "LOW");

            QLabel* label = sensorLabels[sensor.id];
            label->setText(text);

            if (inAlarm) {
                label->setStyleSheet("font-size: 22px; padding: 5px; color: #ff6b6b; font-weight: bold;");
            } else {
                label->setStyleSheet("font-size: 22px; padding: 5px; color: #4ade80;");
            }
        }
    }

    // Update alarm count
    int alarmCount = alarmManager.getAlarmCount();
    alarmCountLabel->setText(QString("Alarms: %1").arg(alarmCount));
    if (alarmCount > 0) {
        alarmBellLabel->setPixmap(bellPixmap(QColor("#ff6b6b"), 30));
        alarmCountLabel->setStyleSheet("font-size: 26px; font-weight: bold; color: #ff6b6b;");
        diagnoseButton->show();
    } else {
        alarmBellLabel->setPixmap(bellPixmap(QColor("#4ade80"), 30));
        alarmCountLabel->setStyleSheet("font-size: 26px; font-weight: bold; color: #4ade80;");
        diagnoseButton->hide();
    }

    // Update chart
    updateChart();
}

void MainWindow::onTriggerAlarm() {
    QPushButton* btn = qobject_cast<QPushButton*>(sender());
    if (!btn) return;

    QString sensorId = btn->property("sensorId").toString();
    QString alarmType = btn->property("alarmType").toString();

    alarmManager.triggerAlarm(sensorId.toStdString(), alarmType.toStdString());
    updateDisplay();
}

void MainWindow::onResetAlarms() {
    alarmManager.resetAlarms();
    updateDisplay();
}

void MainWindow::onTimeRangeSelected(int index) {
    if (index >= 0) {
        selectedTimeRange = timeRangeComboBox->itemData(index).toInt();
        updateChart();
    }
}

void MainWindow::onDiagnoseClicked() {
    // Get all active alarms
    auto alarms = alarmManager.getActiveAlarms();
    if (alarms.empty()) return;

    // Build comma-separated alarm list
    QStringList alarmList;
    for (const auto& alarm : alarms) {
        QString alarmName = QString::fromStdString(alarm.sensorName);
        alarmList.append(alarmName);
    }
    QString alarmsParam = alarmList.join(",");

    // Build URL
    QString url = QString("https://www.srs-enterprises.com/?alarms=%1").arg(QUrl::toPercentEncoding(alarmsParam).constData());

    // Generate QR code
    QRcode* qr = QRcode_encodeString(url.toUtf8().constData(), 0, QR_ECLEVEL_M, QR_MODE_8, 1);
    if (!qr) return;

    // Convert to QImage
    int scale = 8;
    int size = qr->width * scale;
    QImage qrImage(size, size, QImage::Format_RGB32);
    qrImage.fill(Qt::white);

    for (int y = 0; y < qr->width; y++) {
        for (int x = 0; x < qr->width; x++) {
            if (qr->data[y * qr->width + x] & 1) {
                for (int sy = 0; sy < scale; sy++) {
                    for (int sx = 0; sx < scale; sx++) {
                        qrImage.setPixel(x * scale + sx, y * scale + sy, qRgb(0, 0, 0));
                    }
                }
            }
        }
    }
    QRcode_free(qr);

    // Create popup dialog
    QDialog* dialog = new QDialog(this);
    dialog->setWindowTitle("Diagnose Alarms");
    dialog->setStyleSheet("background: white;");

    QVBoxLayout* layout = new QVBoxLayout(dialog);

    QLabel* titleLabel = new QLabel("Scan QR Code to Diagnose", dialog);
    titleLabel->setStyleSheet("font-size: 16px; font-weight: bold; color: black; padding: 10px;");
    titleLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(titleLabel);

    QLabel* qrLabel = new QLabel(dialog);
    qrLabel->setPixmap(QPixmap::fromImage(qrImage));
    qrLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(qrLabel);

    QLabel* alarmsLabel = new QLabel(QString("Active alarms: %1").arg(alarmsParam), dialog);
    alarmsLabel->setStyleSheet("font-size: 12px; color: #666; padding: 10px;");
    alarmsLabel->setAlignment(Qt::AlignCenter);
    alarmsLabel->setWordWrap(true);
    layout->addWidget(alarmsLabel);

    QPushButton* closeButton = new QPushButton("Close", dialog);
    closeButton->setStyleSheet("background: #16213e; color: white; padding: 10px 20px;");
    connect(closeButton, &QPushButton::clicked, dialog, &QDialog::accept);
    layout->addWidget(closeButton);

    dialog->exec();
    delete dialog;
}

void MainWindow::updateChart() {
    float globalMin = std::numeric_limits<float>::max();
    float globalMax = std::numeric_limits<float>::lowest();
    bool hasData = false;
    const auto& activeSeries = chartView->chart()->series();

    for (auto& [id, series] : chartSeriesMap) {
        if (!activeSeries.contains(series)) continue;

        const auto& history = sensorManager.getHistory(id);
        series->clear();
        if (history.empty()) continue;

        size_t pointsToShow = std::min(static_cast<size_t>(selectedTimeRange), history.size());
        size_t startIdx = history.size() - pointsToShow;

        for (size_t i = startIdx; i < history.size(); i++) {
            float value = history[i];
            series->append(static_cast<int>(i - startIdx), value);
            if (value < globalMin) globalMin = value;
            if (value > globalMax) globalMax = value;
            hasData = true;
        }
    }

    if (hasData) {
        axisY->setRange(globalMin - 10.0, globalMax + 10.0);
    }
    axisX->setRange(0, selectedTimeRange - 1);
}

void MainWindow::updateLegend() {
    QHBoxLayout* layout = qobject_cast<QHBoxLayout*>(legendWidget->layout());
    if (!layout) return;

    // Remove all existing items
    while (QLayoutItem* item = layout->takeAt(0)) {
        if (QWidget* w = item->widget()) w->deleteLater();
        delete item;
    }

    bool anyChecked = false;
    for (auto& [id, cb] : sensorCheckBoxes) {
        if (!cb->isChecked()) continue;
        anyChecked = true;

        QLineSeries* series = chartSeriesMap[id];

        QLabel* swatch = new QLabel(legendWidget);
        swatch->setFixedSize(14, 14);
        swatch->setStyleSheet(QString("background-color: %1; border-radius: 3px;")
                                  .arg(series->color().name()));
        layout->addWidget(swatch, 0, Qt::AlignVCenter);

        QLabel* nameLabel = new QLabel(series->name(), legendWidget);
        nameLabel->setStyleSheet("color: white; font-size: 14px; background: transparent;");
        layout->addWidget(nameLabel, 0, Qt::AlignVCenter);

        layout->addSpacing(16);
    }

    if (!anyChecked) {
        QLabel* empty = new QLabel("Select sensors to display", legendWidget);
        empty->setStyleSheet("color: #606080; font-size: 14px; background: transparent;");
        layout->addWidget(empty);
    }

    layout->addStretch();
}

bool MainWindow::event(QEvent* event) {
    if (event->type() == QEvent::Gesture) {
        QGestureEvent* gestureEvent = static_cast<QGestureEvent*>(event);
        if (QPinchGesture* pinch = static_cast<QPinchGesture*>(gestureEvent->gesture(Qt::PinchGesture))) {
            handlePinchGesture(pinch);
            return true;
        }
    }
    return QMainWindow::event(event);
}

bool MainWindow::eventFilter(QObject* obj, QEvent* event) {
    if ((obj == pdfView || obj == pdfView->viewport()) && event->type() == QEvent::Gesture) {
        QGestureEvent* gestureEvent = static_cast<QGestureEvent*>(event);
        if (QPinchGesture* pinch = static_cast<QPinchGesture*>(gestureEvent->gesture(Qt::PinchGesture))) {
            handlePinchGesture(pinch);
            return true;
        }
    }
    if (obj == videoClickArea && event->type() == QEvent::MouseButtonPress) {
        if (videoPlayer->playbackState() == QMediaPlayer::PlayingState)
            videoPlayer->pause();
        else
            videoPlayer->play();
        return true;
    }
    return QMainWindow::eventFilter(obj, event);
}

void MainWindow::handlePinchGesture(QPinchGesture* gesture) {
    qreal scaleFactor = gesture->scaleFactor();
    pdfView->scale(scaleFactor, scaleFactor);
}

void MainWindow::onVideoSelected(QListWidgetItem* item) {
    QString file = item->data(Qt::UserRole).toString();
    if (file.isEmpty()) {
        videoPlayer->stop();
        videoContainer->hide();
        videoControlsBar->hide();
        videoUnavailableLabel->setText("\"" + item->text() + "\" — coming soon");
        videoUnavailableLabel->show();
        return;
    }
    videoUnavailableLabel->hide();
    videoContainer->show();
    videoControlsBar->show();
    videoSeekSlider->setValue(0);
    videoTimeLabel->setText("0:00 / 0:00");
    videoPauseButton->setText("⏸");
    videoPlayer->setSource(QUrl::fromLocalFile(file));
    videoPlayer->play();
}

void MainWindow::onGenerateOrder() {
    QString partNumber = orderPartNumber->text().trimmed();
    QString partName   = orderPartName->text().trimmed();
    QString quantity   = orderQuantity->text().trimmed();

    if (partNumber.isEmpty() || quantity.isEmpty()) return;

    // Add row to order table
    int row = orderTable->rowCount();
    orderTable->insertRow(row);
    auto makeCell = [](const QString& text) {
        auto* cell = new QTableWidgetItem(text);
        cell->setTextAlignment(Qt::AlignCenter);
        return cell;
    };
    orderTable->setItem(row, 0, makeCell(partNumber));
    orderTable->setItem(row, 1, makeCell(partName));
    orderTable->setItem(row, 2, makeCell(quantity));
    orderTable->setRowHeight(row, 44);

    // Clear form
    orderPartNumber->clear();
    orderPartName->clear();
    orderQuantity->clear();

    // Build URL encoding all items in the order
    QStringList items;
    for (int i = 0; i < orderTable->rowCount(); i++) {
        items << orderTable->item(i, 0)->text()
               + "|" + orderTable->item(i, 1)->text()
               + "|" + orderTable->item(i, 2)->text();
    }
    QString url = "https://www.srs-enterprises.com/order?items="
                  + QString(QUrl::toPercentEncoding(items.join(",")));

    QRcode* qr = QRcode_encodeString(url.toUtf8().constData(), 0, QR_ECLEVEL_M, QR_MODE_8, 1);
    if (!qr) return;

    int scale = 5;
    int size = qr->width * scale;
    QImage qrImage(size, size, QImage::Format_RGB32);
    qrImage.fill(Qt::white);
    for (int y = 0; y < qr->width; y++)
        for (int x = 0; x < qr->width; x++)
            if (qr->data[y * qr->width + x] & 1)
                for (int sy = 0; sy < scale; sy++)
                    for (int sx = 0; sx < scale; sx++)
                        qrImage.setPixel(x * scale + sx, y * scale + sy, qRgb(0, 0, 0));
    QRcode_free(qr);

    orderQrLabel->setPixmap(QPixmap::fromImage(qrImage));
    orderQrHint->show();
}

void MainWindow::onSaveThresholds() {
    for (auto& [sensorId, spin] : thresholdLowEdits) {
        alarmManager.setLowThreshold(sensorId, spin->value());
    }
    for (auto& [sensorId, spin] : thresholdHighEdits) {
        alarmManager.setHighThreshold(sensorId, spin->value());
    }
}
