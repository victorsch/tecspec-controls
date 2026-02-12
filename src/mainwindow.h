#pragma once

#include <QMainWindow>
#include <QTimer>
#include <QLabel>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QLineEdit>
#include <QDoubleSpinBox>
#include <QPushButton>
#include <QComboBox>
#include <QtCharts/QChartView>
#include <QtCharts/QLineSeries>
#include <QtCharts/QValueAxis>
#include <QScrollArea>
#include <QTableWidget>
#include <QGraphicsView>
#include <QGraphicsScene>
#include <map>
#include <memory>

class QPinchGesture;

namespace Poppler {
    class Document;
}



class SensorManager;
class AlarmManager;
class BACnetInterface;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    MainWindow(SensorManager& sensors, AlarmManager& alarms,
               BACnetInterface& bacnet, QWidget* parent = nullptr);
    ~MainWindow();

private slots:
    void onUpdateTimer();
    void onBACnetTimer();
    void onTriggerAlarm();
    void onResetAlarms();
    void onSensorSelected(int index);
    void onTimeRangeSelected(int index);
    void onDiagnoseClicked();

private:
    void setupUI();
    void updateDisplay();
    void updateChart();

    SensorManager& sensorManager;
    AlarmManager& alarmManager;
    BACnetInterface& bacnetInterface;

    QTimer* updateTimer;
    QTimer* bacnetTimer;

    // UI elements
    std::map<std::string, QLabel*> sensorLabels;
    QLabel* alarmCountLabel;
    QPushButton* diagnoseButton;
    QLabel* logoLabel;

    QTabWidget* tabWidget;

    // Chart components
    QComboBox* sensorComboBox;
    QComboBox* timeRangeComboBox;
    QChartView* chartView;
    QLineSeries* chartSeries;
    QValueAxis* axisX;
    QValueAxis* axisY;
    std::string selectedSensorId;
    int selectedTimeRange;  // seconds

    // PDF viewer components
    QGraphicsView* pdfView;
    QGraphicsScene* pdfScene;
    std::unique_ptr<Poppler::Document> pdfDocument;
    bool event(QEvent* event) override;
    bool eventFilter(QObject* obj, QEvent* event) override;
    void handlePinchGesture(QPinchGesture* gesture);

    // Parts list components
    QTableWidget* partsTable;
    QLineEdit* orderPartNumber;
    QLineEdit* orderPartName;
    QLineEdit* orderQuantity;
    QLabel* orderQrLabel;
    QLabel* orderQrHint;

    // Settings tab components
    std::map<std::string, QDoubleSpinBox*> thresholdLowEdits;
    std::map<std::string, QDoubleSpinBox*> thresholdHighEdits;

    void onGenerateOrder();
    void onSaveThresholds();
};
