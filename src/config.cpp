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
    display.width = 1280;
    display.height = 800;
    display.fullscreen = false;
    display.devMode = false;  // Set to true for development
    display.scaleFactor = 1.0f;

    // BACnet settings
    bacnet.device_id = 1001;
    bacnet.device_name = "Smart_Heatex";
    bacnet.vendor_id = 999;
    bacnet.ip_address = "0.0.0.0";
    bacnet.ip_port = 47808;

    // Device info
    deviceInfo.model = "HX-5000-A";
    deviceInfo.customer = "";
    deviceInfo.configType = "Standard";
    deviceInfo.contactName = "Demo User";
    deviceInfo.contactEmail = "demo@srs-enterprises.com";
    deviceInfo.contactPhone = "555-555-5555";
    deviceInfo.company = "SRS";

    // Sensor configurations
    sensors = {
        {"inlet_temp_hot", "Hot Inlet Temperature", "°F", 32, 212, 185, 50},
        {"outlet_temp_hot", "Hot Outlet Temperature", "°F", 32, 212, 176, 59},
        {"inlet_temp_cold", "Cold Inlet Temperature", "°F", 32, 122, 104, 41},
        {"outlet_temp_cold", "Cold Outlet Temperature", "°F", 32, 122, 113, 41},
        {"flow_rate_hot", "Hot Side Flow Rate", "GPM", 0, 300.0f, 210.0f, 150.0f},
        {"flow_rate_cold", "Cold Side Flow Rate", "GPM", 0, 300.0f, 210.0f, 150.0f},
        {"pressure_hot_inlet", "Hot Inlet Pressure", "psi", 0, 145, 116, 7.25f},
        {"pressure_hot_outlet", "Hot Outlet Pressure", "psi", 0, 145, 116, 7.25f},
        {"pressure_cold_inlet", "Cold Inlet Pressure", "psi", 0, 145, 116, 7.25f},
        {"pressure_cold_outlet", "Cold Outlet Pressure", "psi", 0, 145, 116, 7.25f}
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
