#pragma once

#include <map>
#include <string>
#include <deque>
#include <random>

class SensorManager {
public:
    SensorManager();

    void update();
    float getValue(const std::string& sensorId) const;
    std::map<std::string, float> getAllValues() const;
    float calculateEfficiency() const;
    const std::deque<float>& getHistory(const std::string& sensorId) const;

private:
    static constexpr size_t MAX_HISTORY = 300;  // 5 minutes at 1Hz

    std::map<std::string, float> values;
    std::map<std::string, float> baseValues;
    std::map<std::string, std::deque<float>> history;
    std::mt19937 rng;
    std::normal_distribution<float> noise;

    void initializeBaseValues();
    void updateCalculatedValues();
    static float clamp(float value, float min, float max);
};
