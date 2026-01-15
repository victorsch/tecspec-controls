#pragma once

#include <QMainWindow>
#include <QTimer>
#include <QLabel>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QComboBox>
#include <QtCharts/QChartView>
#include <QtCharts/QLineSeries>
#include <QtCharts/QValueAxis>
#include <map>
#include <memory>



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
};
