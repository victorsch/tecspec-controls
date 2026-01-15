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
    static AlarmManager* alarmManagerPtr;  // For callback access
    static std::map<uint32_t, std::pair<std::string, bool>> instanceToSensorMap;  // Maps AV instance to (sensorId, isHigh)

    void initServiceHandlers();
    void createObjects();
    static void handleAlarmSetpointWrite(uint32_t instance, float oldValue, float newValue);
};
