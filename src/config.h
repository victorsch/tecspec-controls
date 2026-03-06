#pragma once

#include <string>
#include <map>
#include <vector>
#include <cstdint>

struct SensorConfig {
    std::string id;
    std::string name;
    std::string unit;
    float min;
    float max;
    float alarm_high;
    float alarm_low;
};

struct BACnetConfig {
    uint32_t device_id;
    std::string device_name;
    uint16_t vendor_id;
    std::string ip_address;
    uint16_t ip_port;
};

struct DisplayConfig {
    int width;
    int height;
    bool fullscreen;
    bool devMode;
    bool hideCursor;
    float scaleFactor;
};

struct DeviceInfo {
    std::string model;
    std::string customer;
    std::string configType;
    std::string contactName;
    std::string contactEmail;
    std::string contactPhone;
    std::string company;
};

class Config {
public:
    static Config& instance();

    const std::vector<SensorConfig>& getSensors() const { return sensors; }
    const BACnetConfig& getBACnetConfig() const { return bacnet; }
    const DisplayConfig& getDisplayConfig() const { return display; }
    const DeviceInfo& getDeviceInfo() const { return deviceInfo; }

    const SensorConfig* getSensorById(const std::string& id) const;

private:
    Config();

    std::vector<SensorConfig> sensors;
    BACnetConfig bacnet;
    DisplayConfig display;
    DeviceInfo deviceInfo;

    void initDefaults();
};
