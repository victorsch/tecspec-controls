#include <QApplication>
#include "mainwindow.h"
#include "config.h"
#include "sensor_manager.h"
#include "alarm_manager.h"
#include "bacnet_interface.h"
#include <cstdio>

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);

    printf("Heat Exchanger BMS - C++ Production Build\n");
    printf("==========================================\n\n");

    // Get configuration
    const auto& bacnetConfig = Config::instance().getBACnetConfig();

    // Initialize components
    SensorManager sensorManager;
    AlarmManager alarmManager;
    BACnetInterface bacnetInterface;

    // Initialize BACnet
    if (!bacnetInterface.initialize(bacnetConfig.device_id, bacnetConfig.device_name)) {
        fprintf(stderr, "Failed to initialize BACnet interface\n");
        return 1;
    }

    printf("\nBACnet Device ID: %u\n", bacnetConfig.device_id);
    printf("BACnet Device Name: %s\n", bacnetConfig.device_name.c_str());
    printf("BACnet Port: %u\n\n", bacnetConfig.ip_port);

    // Create and show main window
    MainWindow window(sensorManager, alarmManager, bacnetInterface);
    window.show();

    printf("GUI started. BACnet device is now discoverable on the network.\n");

    // Run Qt event loop
    int result = app.exec();

    // Cleanup
    bacnetInterface.shutdown();

    return result;
}
