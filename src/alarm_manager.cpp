#include "alarm_manager.h"
#include "config.h"

AlarmManager::AlarmManager() {
    // Initialize all alarm states to false
    const auto& sensors = Config::instance().getSensors();
    for (const auto& sensor : sensors) {
        alarmStates[sensor.id + "_HIGH"] = false;
        alarmStates[sensor.id + "_LOW"] = false;
    }
}

void AlarmManager::checkAlarms(const std::map<std::string, float>& sensorValues) {
    // Skip automatic checking when in test mode
    if (testModeActive) return;

    activeAlarms.clear();
    const auto& sensors = Config::instance().getSensors();

    for (const auto& sensor : sensors) {
        auto it = sensorValues.find(sensor.id);
        if (it == sensorValues.end()) continue;

        float value = it->second;

        // Check high alarm
        bool highActive = value >= sensor.alarm_high;
        alarmStates[sensor.id + "_HIGH"] = highActive;
        if (highActive) {
            activeAlarms.push_back({
                sensor.id,
                sensor.name,
                "HIGH",
                value,
                sensor.alarm_high,
                sensor.unit
            });
        }

        // Check low alarm
        bool lowActive = value <= sensor.alarm_low;
        alarmStates[sensor.id + "_LOW"] = lowActive;
        if (lowActive) {
            activeAlarms.push_back({
                sensor.id,
                sensor.name,
                "LOW",
                value,
                sensor.alarm_low,
                sensor.unit
            });
        }
    }
}

std::vector<Alarm> AlarmManager::getActiveAlarms() const {
    return activeAlarms;
}

bool AlarmManager::isAlarmActive(const std::string& sensorId, const std::string& type) const {
    std::string key = sensorId + "_" + type;
    auto it = alarmStates.find(key);
    return (it != alarmStates.end()) ? it->second : false;
}

int AlarmManager::getAlarmCount() const {
    return static_cast<int>(activeAlarms.size());
}

void AlarmManager::triggerAlarm(const std::string& sensorId, const std::string& type) {
    const auto* sensor = Config::instance().getSensorById(sensorId);
    if (!sensor) return;

    testModeActive = true;

    std::string key = sensorId + "_" + type;
    alarmStates[key] = true;

    float threshold = (type == "HIGH") ? sensor->alarm_high : sensor->alarm_low;
    activeAlarms.push_back({
        sensor->id,
        sensor->name,
        type,
        threshold,  // Use threshold as test value
        threshold,
        sensor->unit
    });
}

void AlarmManager::resetAlarms() {
    for (auto& state : alarmStates) {
        state.second = false;
    }
    activeAlarms.clear();
    testModeActive = false;
}
