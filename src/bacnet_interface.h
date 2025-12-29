#pragma once

#include <string>
#include <map>
#include <vector>
#include <cstdint>

class SensorManager;
class AlarmManager;

class BACnetInterface {
public:
    BACnetInterface();
    ~BACnetInterface();

    bool initialize(uint32_t deviceId, const std::string& deviceName);
    void shutdown();
    void process();  // Call periodically to process BACnet messages
    void updateValues(SensorManager& sensors, AlarmManager& alarms);

    bool isRunning() const { return running; }

private:
    bool running;
    std::vector<std::string> alarmNames;  // Persistent storage for BI names

    void initServiceHandlers();
    void createObjects();
};
