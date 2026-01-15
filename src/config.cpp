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
        {"inlet_temp_hot", "Hot Inlet Temperature", "°F", 32, 212, 185, 50},
        {"outlet_temp_hot", "Hot Outlet Temperature", "°F", 32, 212, 176, 59},
        {"inlet_temp_cold", "Cold Inlet Temperature", "°F", 32, 122, 104, 41},
        {"outlet_temp_cold", "Cold Outlet Temperature", "°F", 32, 122, 113, 41},
        {"flow_rate_hot", "Hot Side Flow Rate", "GPM", 0, 52.8f, 47.6f, 5.3f},
        {"flow_rate_cold", "Cold Side Flow Rate", "GPM", 0, 52.8f, 47.6f, 5.3f},
        {"pressure_hot", "Hot Side Pressure", "psi", 0, 145, 116, 7.25f},
        {"pressure_cold", "Cold Side Pressure", "psi", 0, 145, 116, 7.25f}
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
