#include "config.h"

Config& Config::instance() {
    static Config instance;
    return instance;
}

Config::Config() {
    initDefaults();
}

void Config::initDefaults() {
    // Display settings
    display.width = 800;
    display.height = 480;
    display.fullscreen = false;
    display.devMode = true;  // Set to false for production

    // BACnet settings
    bacnet.device_id = 1001;
    bacnet.device_name = "Notus_One_Dev_v1";
    bacnet.vendor_id = 999;
    bacnet.ip_address = "0.0.0.0";
    bacnet.ip_port = 47808;

    // Sensor configurations
    sensors = {
        {"inlet_temp_hot", "Hot Inlet Temperature", "°C", 0, 100, 85, 10},
        {"outlet_temp_hot", "Hot Outlet Temperature", "°C", 0, 100, 80, 15},
        {"inlet_temp_cold", "Cold Inlet Temperature", "°C", 0, 50, 40, 5},
        {"outlet_temp_cold", "Cold Outlet Temperature", "°C", 0, 50, 45, 5},
        {"flow_rate_hot", "Hot Side Flow Rate", "L/min", 0, 200, 180, 20},
        {"flow_rate_cold", "Cold Side Flow Rate", "L/min", 0, 200, 180, 20},
        {"pressure_hot", "Hot Side Pressure", "bar", 0, 10, 8, 0.5f},
        {"pressure_cold", "Cold Side Pressure", "bar", 0, 10, 8, 0.5f}
    };
}

const SensorConfig* Config::getSensorById(const std::string& id) const {
    for (const auto& sensor : sensors) {
        if (sensor.id == id) {
            return &sensor;
        }
    }
    return nullptr;
}
