#include "sensor_manager.h"
#include "config.h"
#include <cmath>
#include <chrono>

SensorManager::SensorManager()
    : rng(std::chrono::steady_clock::now().time_since_epoch().count())
    , noise(0.0f, 0.5f) {
    initializeBaseValues();
}

void SensorManager::initializeBaseValues() {
    // Set reasonable base values for simulation
    baseValues["inlet_temp_hot"] = 149.0f;      // °F
    baseValues["outlet_temp_hot"] = 113.0f;     // °F
    baseValues["inlet_temp_cold"] = 59.0f;      // °F
    baseValues["outlet_temp_cold"] = 95.0f;     // °F
    baseValues["flow_rate_hot"] = 200.0f;       // GPM
    baseValues["flow_rate_cold"] = 200.0f;      // GPM
    baseValues["pressure_hot_inlet"] = 58.0f;    // psi
    baseValues["pressure_hot_outlet"] = 54.0f;  // psi
    baseValues["pressure_cold_inlet"] = 50.8f;  // psi
    baseValues["pressure_cold_outlet"] = 47.0f; // psi

    // Initialize current values
    for (const auto& [id, base] : baseValues) {
        values[id] = base;
    }
}

void SensorManager::update() {
    const auto& config = Config::instance();

    // Add time-based drift and noise to simulate real sensors
    auto now = std::chrono::steady_clock::now().time_since_epoch();
    float time = std::chrono::duration<float>(now).count();

    for (const auto& sensor : config.getSensors()) {
        float base = baseValues[sensor.id];

        // Add slow drift (sine wave) and random noise
        float drift = std::sin(time * 0.1f + std::hash<std::string>{}(sensor.id)) * 2.0f;
        float noiseVal = noise(rng);

        float newValue = base + drift + noiseVal;
        values[sensor.id] = clamp(newValue, sensor.min, sensor.max);

        // Store in history
        auto& hist = history[sensor.id];
        hist.push_back(values[sensor.id]);
        if (hist.size() > MAX_HISTORY) {
            hist.pop_front();
        }
    }

    // Update calculated values
    updateCalculatedValues();
}

void SensorManager::updateCalculatedValues() {
    // Calculate BTUs: GPM x 500 x delta T
    float gpm = getValue("flow_rate_hot");
    float inletTemp = getValue("inlet_temp_hot");
    float outletTemp = getValue("outlet_temp_hot");
    float deltaT = inletTemp - outletTemp;

    float btus = gpm * 500.0f * deltaT;
    values["btus"] = btus;
    auto& hist = history["btus"];
    hist.push_back(btus);
    if (hist.size() > MAX_HISTORY) hist.pop_front();

    // Hot side pressure differential
    float hotDiff = getValue("pressure_hot_inlet") - getValue("pressure_hot_outlet");
    values["pressure_diff_hot"] = hotDiff;
    auto& histHD = history["pressure_diff_hot"];
    histHD.push_back(hotDiff);
    if (histHD.size() > MAX_HISTORY) histHD.pop_front();

    // Cold side pressure differential
    float coldDiff = getValue("pressure_cold_inlet") - getValue("pressure_cold_outlet");
    values["pressure_diff_cold"] = coldDiff;
    auto& histCD = history["pressure_diff_cold"];
    histCD.push_back(coldDiff);
    if (histCD.size() > MAX_HISTORY) histCD.pop_front();
}

float SensorManager::getValue(const std::string& sensorId) const {
    auto it = values.find(sensorId);
    return (it != values.end()) ? it->second : 0.0f;
}

std::map<std::string, float> SensorManager::getAllValues() const {
    return values;
}

float SensorManager::calculateEfficiency() const {
    float inletHot = getValue("inlet_temp_hot");
    float outletHot = getValue("outlet_temp_hot");
    float inletCold = getValue("inlet_temp_cold");

    float maxTransfer = inletHot - inletCold;
    if (maxTransfer <= 0) return 0.0f;

    float actualTransfer = inletHot - outletHot;
    float efficiency = (actualTransfer / maxTransfer) * 100.0f;

    return clamp(efficiency, 0.0f, 100.0f);
}

float SensorManager::clamp(float value, float min, float max) {
    if (value < min) return min;
    if (value > max) return max;
    return value;
}

const std::deque<float>& SensorManager::getHistory(const std::string& sensorId) const {
    static std::deque<float> empty;
    auto it = history.find(sensorId);
    return (it != history.end()) ? it->second : empty;
}
