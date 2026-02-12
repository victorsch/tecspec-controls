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
#include <QGestureEvent>
#include <QGraphicsPixmapItem>
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
            padding: 18px 36px;
            margin-right: 2px;
            border-top-left-radius: 6px;
            border-top-right-radius: 6px;
            font-size: 20px;
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
    QLabel* title = new QLabel("Real-time Monitoring", this);
    title->setAlignment(Qt::AlignCenter);
    title->setStyleSheet("font-size: 24px; font-weight: bold; padding: 10px; color: #00d4ff;");
    monitoringLayout->addWidget(title);

    // Status bar
    QHBoxLayout* statusLayout = new QHBoxLayout();

    statusLayout->addStretch();

    alarmCountLabel = new QLabel("Alarms: 0", this);
    alarmCountLabel->setStyleSheet("font-size: 26px; font-weight: bold; color: #4CAF50;");
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
        label->setStyleSheet("font-size: 26px; padding: 10px;");
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
    QString comboStyle = R"(
        QComboBox {
            font-size: 18px;
            padding: 10px 15px;
            min-height: 30px;
            background: #16213e;
            color: white;
            border: 2px solid #0f3460;
            border-radius: 6px;
        }
        QComboBox::drop-down {
            width: 40px;
        }
        QComboBox QAbstractItemView {
            font-size: 28px;
            background: #16213e;
            color: white;
            outline: none;
        }
        QComboBox QAbstractItemView::item {
            min-height: 60px;
            padding: 18px 15px;
            border-bottom: 2px solid #0f3460;
        }
        QComboBox QAbstractItemView::item:selected {
            background-color: #00d4ff;
            color: #1a1a2e;
        }
        QComboBox QAbstractItemView::item:hover {
            background-color: #1f4068;
        }
    )";

    QHBoxLayout* selectorLayout = new QHBoxLayout();
    QLabel* selectorLabel = new QLabel("Select Sensor:", this);
    selectorLabel->setStyleSheet("font-size: 18px;");
    selectorLayout->addWidget(selectorLabel);

    sensorComboBox = new QComboBox(this);
    sensorComboBox->setStyleSheet(comboStyle);
    for (const auto& sensor : sensorConfigs) {
        sensorComboBox->addItem(QString::fromStdString(sensor.name),
                                QString::fromStdString(sensor.id));
    }
    // BTUs
    sensorComboBox->addItem(QString::fromStdString("BTUs"), QString::fromStdString("btus"));

    connect(sensorComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MainWindow::onSensorSelected);
    selectorLayout->addWidget(sensorComboBox);

    // Set item heights programmatically for sensor combo
    for (int i = 0; i < sensorComboBox->count(); i++) {
        sensorComboBox->setItemData(i, QSize(0, 65), Qt::SizeHintRole);
    }

    selectorLayout->addSpacing(20);

    QLabel* timeLabel = new QLabel("Time Range:", this);
    timeLabel->setStyleSheet("font-size: 18px;");
    selectorLayout->addWidget(timeLabel);

    timeRangeComboBox = new QComboBox(this);
    timeRangeComboBox->setStyleSheet(comboStyle);
    timeRangeComboBox->addItem("30 seconds", 30);
    timeRangeComboBox->addItem("1 minute", 60);
    timeRangeComboBox->addItem("2 minutes", 120);
    timeRangeComboBox->addItem("5 minutes", 300);
    timeRangeComboBox->setCurrentIndex(1);  // Default to 1 minute
    selectedTimeRange = 60;
    connect(timeRangeComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MainWindow::onTimeRangeSelected);
    selectorLayout->addWidget(timeRangeComboBox);

    // Set item heights programmatically for time combo
    for (int i = 0; i < timeRangeComboBox->count(); i++) {
        timeRangeComboBox->setItemData(i, QSize(0, 65), Qt::SizeHintRole);
    }

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

    // IOM tab (PDF viewer for installation/operation manual)
    const auto& displayConfig = Config::instance().getDisplayConfig();

    QWidget* iomTab = new QWidget();
    QVBoxLayout* iomLayout = new QVBoxLayout(iomTab);
    iomLayout->setContentsMargins(0, 0, 0, 0);

    pdfScene = new QGraphicsScene(this);
    pdfView = new QGraphicsView(pdfScene, this);
    pdfView->setDragMode(QGraphicsView::ScrollHandDrag);
    pdfView->setRenderHint(QPainter::Antialiasing);
    pdfView->setRenderHint(QPainter::SmoothPixmapTransform);
    pdfView->setStyleSheet("QGraphicsView { border: none; background: #2a2a3e; }");
    pdfView->grabGesture(Qt::PinchGesture);
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
    QHBoxLayout* partsColumnsLayout = new QHBoxLayout(partsTab);
    partsColumnsLayout->setSpacing(20);

    // Left column: Order form
    QGroupBox* orderGroup = new QGroupBox("Order Parts", this);
    orderGroup->setStyleSheet(R"(
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
    QVBoxLayout* orderLayout = new QVBoxLayout(orderGroup);
    orderLayout->setSpacing(12);

    QString inputStyle = R"(
        QLineEdit {
            background: #16213e;
            border: 2px solid #0f3460;
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
    partLabel->setStyleSheet("font-size: 16px; color: #e0e0e0;");
    orderLayout->addWidget(partLabel);

    orderPartNumber = new QLineEdit(this);
    orderPartNumber->setPlaceholderText("Enter part number");
    orderPartNumber->setStyleSheet(inputStyle);
    orderLayout->addWidget(orderPartNumber);

    QLabel* nameLabel = new QLabel("Part Name:", this);
    nameLabel->setStyleSheet("font-size: 16px; color: #e0e0e0;");
    orderLayout->addWidget(nameLabel);

    orderPartName = new QLineEdit(this);
    orderPartName->setPlaceholderText("Enter part name");
    orderPartName->setStyleSheet(inputStyle);
    orderLayout->addWidget(orderPartName);

    QLabel* qtyLabel = new QLabel("Quantity:", this);
    qtyLabel->setStyleSheet("font-size: 16px; color: #e0e0e0;");
    orderLayout->addWidget(qtyLabel);

    orderQuantity = new QLineEdit(this);
    orderQuantity->setPlaceholderText("Enter quantity");
    orderQuantity->setStyleSheet(inputStyle);
    orderLayout->addWidget(orderQuantity);

    QPushButton* generateBtn = new QPushButton("Generate Order Form", this);
    generateBtn->setStyleSheet(R"(
        QPushButton {
            background: #0f3460;
            color: #00d4ff;
            font-size: 18px;
            font-weight: bold;
            padding: 12px;
            border-radius: 6px;
            border: 2px solid #00d4ff;
        }
        QPushButton:pressed {
            background: #1a1a2e;
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

    // Right column: Parts table
    QWidget* tableContainer = new QWidget();
    QVBoxLayout* tableLayout = new QVBoxLayout(tableContainer);
    tableLayout->setContentsMargins(0, 0, 0, 0);

    partsTable = new QTableWidget(this);
    partsTable->setColumnCount(3);
    partsTable->setHorizontalHeaderLabels({"Item No", "Item Description", "Qty"});
    partsTable->verticalHeader()->setVisible(false);
    partsTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    partsTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    partsTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    partsTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    partsTable->setSelectionMode(QAbstractItemView::SingleSelection);
    partsTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    partsTable->setAlternatingRowColors(true);
    partsTable->setStyleSheet(R"(
        QTableWidget {
            background-color: #1a1a2e;
            alternate-background-color: #16213e;
            gridline-color: #0f3460;
            font-size: 16px;
        }
        QTableWidget::item {
            padding: 12px 8px;
        }
        QTableWidget::item:selected {
            background-color: #0f3460;
        }
        QHeaderView::section {
            background-color: #0f3460;
            color: #00d4ff;
            padding: 12px;
            border: 1px solid #1a1a2e;
            font-weight: bold;
            font-size: 16px;
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
                partsTable->setRowHeight(row, 48);
            }
        }
        csvFile.close();
    }

    // Click row to fill order form, then deselect after brief highlight
    connect(partsTable, &QTableWidget::cellPressed, this, [this](int row, int) {
        partsTable->selectRow(row);
        QTableWidgetItem* partItem = partsTable->item(row, 0);
        QTableWidgetItem* nameItem = partsTable->item(row, 1);
        QTableWidgetItem* qtyItem = partsTable->item(row, 2);
        if (partItem) orderPartNumber->setText(partItem->text());
        if (nameItem) orderPartName->setText(nameItem->text());
        if (qtyItem) orderQuantity->setText(qtyItem->text());
        QTimer::singleShot(150, this, [this]() {
            partsTable->clearSelection();
        });
    });

    tableLayout->addWidget(partsTable);
    QScroller::grabGesture(partsTable->viewport(), QScroller::TouchGesture);

    partsColumnsLayout->addWidget(orderGroup, 1);
    partsColumnsLayout->addWidget(tableContainer, 2);
    tabWidget->addTab(partsTab, "Parts List");
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
                label->setStyleSheet("font-size: 26px; padding: 10px; color: #ff6b6b; font-weight: bold;");
            } else {
                label->setStyleSheet("font-size: 26px; padding: 10px; color: #4ade80;");
            }
        }
    }

    // Update alarm count
    int alarmCount = alarmManager.getAlarmCount();
    alarmCountLabel->setText(QString("Alarms: %1").arg(alarmCount));
    if (alarmCount > 0) {
        alarmCountLabel->setStyleSheet("font-size: 26px; font-weight: bold; color: #ff6b6b;");
        diagnoseButton->show();
    } else {
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

void MainWindow::handlePinchGesture(QPinchGesture* gesture) {
    qreal scaleFactor = gesture->scaleFactor();
    pdfView->scale(scaleFactor, scaleFactor);
}

void MainWindow::onGenerateOrder() {
    QString partNumber = orderPartNumber->text().trimmed();
    QString partName = orderPartName->text().trimmed();
    QString quantity = orderQuantity->text().trimmed();

    if (partNumber.isEmpty() || quantity.isEmpty()) return;

    QString url = QString("https://www.srs-enterprises.com?part=%1&name=%2&qty=%3")
        .arg(QUrl::toPercentEncoding(partNumber).constData())
        .arg(QUrl::toPercentEncoding(partName).constData())
        .arg(QUrl::toPercentEncoding(quantity).constData());

    QRcode* qr = QRcode_encodeString(url.toUtf8().constData(), 0, QR_ECLEVEL_M, QR_MODE_8, 1);
    if (!qr) return;

    int scale = 6;
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

    orderQrLabel->setPixmap(QPixmap::fromImage(qrImage));
    orderQrHint->show();
}
