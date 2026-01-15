#include "alarm_manager.h"
#include "config.h"

AlarmManager::AlarmManager() {
    // Initialize all alarm states to false and set default thresholds
    const auto& sensors = Config::instance().getSensors();
    for (const auto& sensor : sensors) {
        alarmStates[sensor.id + "_HIGH"] = false;
        alarmStates[sensor.id + "_LOW"] = false;
        dynamicHighThresholds[sensor.id] = sensor.alarm_high;
        dynamicLowThresholds[sensor.id] = sensor.alarm_low;
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

        // Get dynamic thresholds
        float highThreshold = getHighThreshold(sensor.id);
        float lowThreshold = getLowThreshold(sensor.id);

        // Check high alarm
        bool highActive = value >= highThreshold;
        alarmStates[sensor.id + "_HIGH"] = highActive;
        if (highActive) {
            activeAlarms.push_back({
                sensor.id,
                sensor.name,
                "HIGH",
                value,
                highThreshold,
                sensor.unit
            });
        }

        // Check low alarm
        bool lowActive = value <= lowThreshold;
        alarmStates[sensor.id + "_LOW"] = lowActive;
        if (lowActive) {
            activeAlarms.push_back({
                sensor.id,
                sensor.name,
                "LOW",
                value,
                lowThreshold,
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

    float threshold = (type == "HIGH") ? getHighThreshold(sensorId) : getLowThreshold(sensorId);
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

void AlarmManager::setHighThreshold(const std::string& sensorId, float threshold) {
    dynamicHighThresholds[sensorId] = threshold;
}

void AlarmManager::setLowThreshold(const std::string& sensorId, float threshold) {
    dynamicLowThresholds[sensorId] = threshold;
}

float AlarmManager::getHighThreshold(const std::string& sensorId) const {
    auto it = dynamicHighThresholds.find(sensorId);
    if (it != dynamicHighThresholds.end()) {
        return it->second;
    }
    // Fallback to config value if not found
    const auto* sensor = Config::instance().getSensorById(sensorId);
    return sensor ? sensor->alarm_high : 0.0f;
}

float AlarmManager::getLowThreshold(const std::string& sensorId) const {
    auto it = dynamicLowThresholds.find(sensorId);
    if (it != dynamicLowThresholds.end()) {
        return it->second;
    }
    // Fallback to config value if not found
    const auto* sensor = Config::instance().getSensorById(sensorId);
    return sensor ? sensor->alarm_low : 0.0f;
}
