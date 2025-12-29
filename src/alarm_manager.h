#pragma once

#include <string>
#include <vector>
#include <map>

struct Alarm {
    std::string sensorId;
    std::string sensorName;
    std::string type;  // "HIGH" or "LOW"
    float value;
    float threshold;
    std::string unit;
};

class AlarmManager {
public:
    AlarmManager();

    void checkAlarms(const std::map<std::string, float>& sensorValues);
    std::vector<Alarm> getActiveAlarms() const;
    bool isAlarmActive(const std::string& sensorId, const std::string& type) const;
    int getAlarmCount() const;

    // Dev/testing methods
    void triggerAlarm(const std::string& sensorId, const std::string& type);
    void resetAlarms();

private:
    std::map<std::string, bool> alarmStates;  // key: "sensorId_TYPE"
    std::vector<Alarm> activeAlarms;
    bool testModeActive = false;
};
