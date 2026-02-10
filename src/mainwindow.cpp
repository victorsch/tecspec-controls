#include "mainwindow.h"
#include "sensor_manager.h"
#include "alarm_manager.h"
#include "bacnet_interface.h"
#include "config.h"

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
#include <QFile>
#include <QTextStream>
#include <QHeaderView>
#include <QScroller>
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

    // Hide cursor for embedded kiosk mode
    //QApplication::setOverrideCursor(Qt::BlankCursor);

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

void MainWindow::setupUI() {
    QWidget* central = new QWidget(this);
    setCentralWidget(central);

    QVBoxLayout* mainLayout = new QVBoxLayout(central);

    // Tabs
    tabWidget = new QTabWidget();

    // Modern HMI styling
    tabWidget->setStyleSheet(R"(
        QTabWidget::pane {
            border: 1px solid #0f3460;
            background: #1a1a2e;
            border-radius: 8px;
        }
        QTabBar::tab {
            background: #16213e;
            color: #a0a0a0;
            padding: 12px 24px;
            margin-right: 2px;
            border-top-left-radius: 6px;
            border-top-right-radius: 6px;
            font-size: 14px;
            font-weight: bold;
        }
        QTabBar::tab:selected {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                stop:0 #0f3460, stop:1 #1a1a2e);
            color: #00d4ff;
            border-bottom: 2px solid #00d4ff;
        }
        QTabBar::tab:hover:!selected {
            background: #1f4068;
            color: #e0e0e0;
        }
    )");

    // Set overall window style
    central->setStyleSheet(R"(
        QWidget {
            background-color: #1a1a2e;
            color: #e0e0e0;
            font-family: 'Segoe UI', Arial, sans-serif;
        }
        *:focus {
            outline: none;
        }
        QLabel {
            color: #e0e0e0;
        }
        QGroupBox {
            border: 1px solid #0f3460;
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
            background: #16213e;
            color: #e0e0e0;
            border: 1px solid #0f3460;
            border-radius: 4px;
            padding: 8px 16px;
            font-weight: bold;
        }
        QPushButton:hover {
            background: #1f4068;
            border-color: #00d4ff;
        }
        QPushButton:pressed {
            background: #0f3460;
        }
        QComboBox {
            background: #16213e;
            color: #e0e0e0;
            border: 1px solid #0f3460;
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
            background: #16213e;
            color: #e0e0e0;
            selection-background-color: #0f3460;
        }
    )");

    QWidget* monitoringTab = new QWidget();
    QVBoxLayout* monitoringLayout = new QVBoxLayout(monitoringTab);

    QWidget* settingsTab = new QWidget();
    QVBoxLayout* settingsLayout = new QVBoxLayout(settingsTab);

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
    QLabel* title = new QLabel("Heat Exchanger Monitoring", this);
    title->setAlignment(Qt::AlignCenter);
    title->setStyleSheet("font-size: 24px; font-weight: bold; padding: 10px; color: #00d4ff;");
    monitoringLayout->addWidget(title);

    // Status bar
    QHBoxLayout* statusLayout = new QHBoxLayout();

    statusLayout->addStretch();

    alarmCountLabel = new QLabel("Alarms: 0", this);
    alarmCountLabel->setStyleSheet("font-size: 18px; font-weight: bold; color: #4CAF50;");
    statusLayout->addWidget(alarmCountLabel);

    diagnoseButton = new QPushButton("Diagnose", this);
    diagnoseButton->setStyleSheet("background: #ff6b6b; color: white; font-weight: bold;");
    diagnoseButton->hide();  // Initially hidden
    connect(diagnoseButton, &QPushButton::clicked, this, &MainWindow::onDiagnoseClicked);
    statusLayout->addWidget(diagnoseButton);

    monitoringLayout->addLayout(statusLayout);

    // Three-column layout: Hot | Image | Cold
    QHBoxLayout* columnsLayout = new QHBoxLayout();
    columnsLayout->setSpacing(20);

    // Heat exchanger image (center)
    QLabel* imageLabel = new QLabel();
    QPixmap pixmap("../heatex.png");
    imageLabel->setPixmap(pixmap.scaled(400, 500, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    imageLabel->setAlignment(Qt::AlignCenter);

    // Hot side (left column)
    QGroupBox* hotGroup = new QGroupBox("Hot Side", this);
    hotGroup->setStyleSheet(R"(
        QGroupBox {
            font-size: 22px;
            font-weight: bold;
            color: #ff6b6b;
            border: 2px solid #ff6b6b;
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
    QVBoxLayout* hotLayout = new QVBoxLayout(hotGroup);
    hotLayout->setSpacing(15);

    // Cold side (right column)
    QGroupBox* coldGroup = new QGroupBox("Cold Side", this);
    coldGroup->setStyleSheet(R"(
        QGroupBox {
            font-size: 22px;
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
    QVBoxLayout* coldLayout = new QVBoxLayout(coldGroup);
    coldLayout->setSpacing(15);

    const auto& sensorConfigs = Config::instance().getSensors();
    for (const auto& sensor : sensorConfigs) {
        QLabel* label = new QLabel(QString::fromStdString(sensor.name + ": --- " + sensor.unit), this);
        label->setStyleSheet("font-size: 20px; padding: 10px;");
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
    QVBoxLayout* graphsLayout = new QVBoxLayout(graphsTab);

    // Sensor selector dropdown
    QHBoxLayout* selectorLayout = new QHBoxLayout();
    QLabel* selectorLabel = new QLabel("Select Sensor:", this);
    selectorLayout->addWidget(selectorLabel);

    sensorComboBox = new QComboBox(this);
    for (const auto& sensor : sensorConfigs) {
        sensorComboBox->addItem(QString::fromStdString(sensor.name),
                                QString::fromStdString(sensor.id));
    }
    // BTUs
    sensorComboBox->addItem(QString::fromStdString("BTUs"), QString::fromStdString("btus"));

    connect(sensorComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MainWindow::onSensorSelected);
    selectorLayout->addWidget(sensorComboBox);

    selectorLayout->addSpacing(20);

    QLabel* timeLabel = new QLabel("Time Range:", this);
    selectorLayout->addWidget(timeLabel);

    timeRangeComboBox = new QComboBox(this);
    timeRangeComboBox->addItem("30 seconds", 30);
    timeRangeComboBox->addItem("1 minute", 60);
    timeRangeComboBox->addItem("2 minutes", 120);
    timeRangeComboBox->addItem("5 minutes", 300);
    timeRangeComboBox->setCurrentIndex(1);  // Default to 1 minute
    selectedTimeRange = 60;
    connect(timeRangeComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MainWindow::onTimeRangeSelected);
    selectorLayout->addWidget(timeRangeComboBox);

    selectorLayout->addStretch();
    graphsLayout->addLayout(selectorLayout);

    // Create chart
    chartSeries = new QLineSeries();
    QChart* chart = new QChart();
    chart->addSeries(chartSeries);
    chart->setTitle("Sensor Value Over Time");
    chart->legend()->hide();

    // Create axes
    axisX = new QValueAxis();
    axisX->setTitleText("Time (seconds)");
    axisX->setRange(0, 60);
    axisX->setLabelFormat("%d");
    chart->addAxis(axisX, Qt::AlignBottom);
    chartSeries->attachAxis(axisX);

    axisY = new QValueAxis();
    axisY->setTitleText("Value");
    chart->addAxis(axisY, Qt::AlignLeft);
    chartSeries->attachAxis(axisY);

    chartView = new QChartView(chart);
    chartView->setRenderHint(QPainter::Antialiasing);
    graphsLayout->addWidget(chartView);

    // Initialize with first sensor
    if (!sensorConfigs.empty()) {
        selectedSensorId = sensorConfigs[0].id;
    }

    tabWidget->addTab(graphsTab, "Graphs");
    tabWidget->addTab(settingsTab, "Settings");

    // IOM tab (PDF viewer for installation/operation manual)
    const auto& displayConfig = Config::instance().getDisplayConfig();

    QWidget* iomTab = new QWidget();
    QVBoxLayout* iomLayout = new QVBoxLayout(iomTab);
    iomLayout->setContentsMargins(0, 0, 0, 0);

    pdfScrollArea = new QScrollArea(this);
    pdfScrollArea->setWidgetResizable(true);
    pdfScrollArea->setStyleSheet("QScrollArea { border: none; background: #2a2a3e; }");

    pdfContainer = new QWidget();
    pdfPageLayout = new QVBoxLayout(pdfContainer);
    pdfPageLayout->setAlignment(Qt::AlignHCenter);
    pdfPageLayout->setSpacing(10);

    pdfDocument = Poppler::Document::load(QString("../iom.pdf"));
    if (pdfDocument && !pdfDocument->isLocked()) {
        pdfDocument->setRenderHint(Poppler::Document::TextAntialiasing);
        pdfDocument->setRenderHint(Poppler::Document::Antialiasing);

        // Calculate DPI to fit page width to screen (with margin for scrollbar)
        int availableWidth = displayConfig.width - 40;  // Account for scrollbar and margins

        // Render all pages
        for (int i = 0; i < pdfDocument->numPages(); i++) {
            std::unique_ptr<Poppler::Page> page = pdfDocument->page(i);
            if (page) {
                // Page size is in points (1/72 inch)
                QSizeF pageSize = page->pageSizeF();
                double dpi = (availableWidth / pageSize.width()) * 72.0;

                QImage image = page->renderToImage(dpi, dpi);
                QLabel* pageLabel = new QLabel();
                pageLabel->setPixmap(QPixmap::fromImage(image));
                pageLabel->setAlignment(Qt::AlignCenter);
                pdfPageLayout->addWidget(pageLabel);
            }
        }
    } else {
        QLabel* errorLabel = new QLabel("Could not load IOM PDF.\nPlace iom.pdf in the application directory.");
        errorLabel->setAlignment(Qt::AlignCenter);
        errorLabel->setStyleSheet("font-size: 16px; color: #ff6b6b; padding: 20px;");
        pdfPageLayout->addWidget(errorLabel);
    }

    pdfScrollArea->setWidget(pdfContainer);
    iomLayout->addWidget(pdfScrollArea);
    QScroller::grabGesture(pdfScrollArea->viewport(), QScroller::TouchGesture);
    tabWidget->addTab(iomTab, "IOM");

    // Parts List tab
    QWidget* partsTab = new QWidget();
    QVBoxLayout* partsLayout = new QVBoxLayout(partsTab);

    partsTable = new QTableWidget(this);
    partsTable->setColumnCount(3);
    partsTable->setHorizontalHeaderLabels({"Item No", "Item Description", "Quantity"});
    partsTable->verticalHeader()->setVisible(false);  // Hide row numbers
    partsTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    partsTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    partsTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    partsTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    partsTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    partsTable->setAlternatingRowColors(true);
    partsTable->setStyleSheet(R"(
        QTableWidget {
            background-color: #1a1a2e;
            alternate-background-color: #16213e;
            gridline-color: #0f3460;
        }
        QTableWidget::item {
            padding: 8px;
        }
        QTableWidget::item:selected {
            background-color: #0f3460;
        }
        QHeaderView::section {
            background-color: #0f3460;
            color: #00d4ff;
            padding: 10px;
            border: 1px solid #1a1a2e;
            font-weight: bold;
        }
    )");

    // Load CSV data
    QFile csvFile("../parts_list.csv");
    if (csvFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream in(&csvFile);
        bool firstLine = true;
        while (!in.atEnd()) {
            QString line = in.readLine();
            if (firstLine) {
                firstLine = false;
                continue;  // Skip header
            }
            QStringList fields = line.split(',');
            if (fields.size() >= 5) {
                int row = partsTable->rowCount();
                partsTable->insertRow(row);
                partsTable->setItem(row, 0, new QTableWidgetItem(fields[2].trimmed()));  // Item No
                partsTable->setItem(row, 1, new QTableWidgetItem(fields[3].trimmed()));  // Item Description
                partsTable->setItem(row, 2, new QTableWidgetItem(fields[4].trimmed()));  // Quantity
            }
        }
        csvFile.close();
    }

    partsLayout->addWidget(partsTable);
    QScroller::grabGesture(partsTable->viewport(), QScroller::TouchGesture);
    tabWidget->addTab(partsTab, "Parts List");

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

            // Add alarm range if in alarm
            if (inAlarm) {
                float lowThreshold = alarmManager.getLowThreshold(sensor.id);
                float highThreshold = alarmManager.getHighThreshold(sensor.id);
                text += QString(" (Range: %1 - %2)")
                        .arg(lowThreshold, 0, 'f', 1)
                        .arg(highThreshold, 0, 'f', 1);
            }

            QLabel* label = sensorLabels[sensor.id];
            label->setText(text);

            if (inAlarm) {
                label->setStyleSheet("font-size: 20px; padding: 10px; color: #ff6b6b; font-weight: bold;");
            } else {
                label->setStyleSheet("font-size: 20px; padding: 10px; color: #4ade80;");
            }
        }
    }

    // Update alarm count
    int alarmCount = alarmManager.getAlarmCount();
    alarmCountLabel->setText(QString("Alarms: %1").arg(alarmCount));
    if (alarmCount > 0) {
        alarmCountLabel->setStyleSheet("font-size: 18px; font-weight: bold; color: #ff6b6b;");
        diagnoseButton->show();
    } else {
        alarmCountLabel->setStyleSheet("font-size: 18px; font-weight: bold; color: #4ade80;");
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

void MainWindow::onSensorSelected(int index) {
    if (index >= 0) {
        selectedSensorId = sensorComboBox->itemData(index).toString().toStdString();
        updateChart();
    }
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
    if (selectedSensorId.empty()) return;

    const auto& history = sensorManager.getHistory(selectedSensorId);
    chartSeries->clear();

    if (history.empty()) return;

    // Determine how many points to show based on selected time range
    size_t pointsToShow = std::min(static_cast<size_t>(selectedTimeRange), history.size());
    size_t startIdx = history.size() - pointsToShow;

    // Find min/max for Y axis
    float minVal = history[startIdx];
    float maxVal = history[startIdx];

    for (size_t i = startIdx; i < history.size(); i++) {
        float value = history[i];
        // X axis: 0 = oldest visible, pointsToShow-1 = newest
        int xPos = static_cast<int>(i - startIdx);
        chartSeries->append(xPos, value);

        if (value < minVal) minVal = value;
        if (value > maxVal) maxVal = value;
    }

    // Add some padding to Y axis
    float padding = (maxVal - minVal) * 0.1f;
    if (padding < 1.0f) padding = 1.0f;
    axisY->setRange(minVal - padding, maxVal + padding);

    // Set X axis range to match visible data
    axisX->setRange(0, static_cast<int>(pointsToShow - 1));
}
