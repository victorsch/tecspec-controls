#include <QApplication>
#include "mainwindow.h"
#include "config.h"
#include "sensor_manager.h"
#include "alarm_manager.h"
#include "bacnet_interface.h"
#include <cstdio>
#include <cstdlib>

int main(int argc, char* argv[]) {
    // Set scale factor before QApplication is created
    const auto& displayConfig = Config::instance().getDisplayConfig();
    if (displayConfig.scaleFactor != 1.0f) {
        qputenv("QT_SCALE_FACTOR", QByteArray::number(displayConfig.scaleFactor));
    }

    // Hide hardware cursor on embedded platforms (eglfs/linuxfb)
    if (displayConfig.hideCursor) {
        qputenv("QT_QPA_EGLFS_HIDECURSOR", "1");
        qputenv("QT_QPA_FB_HIDECURSOR", "1");
    }

    QApplication app(argc, argv);

    printf("Heat Exchanger BMS - C++ Production Build\n");
    printf("==========================================\n\n");

    // Get configuration
    const auto& bacnetConfig = Config::instance().getBACnetConfig();

    // Initialize components
    SensorManager sensorManager;
    AlarmManager alarmManager;
    BACnetInterface bacnetInterface;

    // Initialize BACnet (non-fatal if it fails)
    if (!bacnetInterface.initialize(bacnetConfig.device_id, bacnetConfig.device_name)) {
        fprintf(stderr, "Warning: BACnet initialization failed - running without network\n");
    } else {
        printf("\nBACnet Device ID: %u\n", bacnetConfig.device_id);
        printf("BACnet Device Name: %s\n", bacnetConfig.device_name.c_str());
        printf("BACnet Port: %u\n\n", bacnetConfig.ip_port);
    }

    // Create and show main window
    MainWindow window(sensorManager, alarmManager, bacnetInterface);
    if (displayConfig.devMode) {
        window.show();
    } else {
        window.showFullScreen();
    }

    printf("GUI started. BACnet device is now discoverable on the network.\n");

    // Run Qt event loop
    int result = app.exec();

    // Cleanup
    bacnetInterface.shutdown();

    return result;
}
