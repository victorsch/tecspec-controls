#include "bacnet_interface.h"
#include "sensor_manager.h"
#include "alarm_manager.h"
#include "config.h"
#include <cstdio>

extern "C" {
#include "bacnet/bacdef.h"
#include "bacnet/apdu.h"
#include "bacnet/npdu.h"
#include "bacnet/dcc.h"
#include "bacnet/iam.h"
#include "bacnet/basic/binding/address.h"
#include "bacnet/basic/services.h"
#include "bacnet/basic/tsm/tsm.h"
#include "bacnet/basic/sys/mstimer.h"
#include "bacnet/datalink/datalink.h"
#include "bacnet/datalink/dlenv.h"
#include "bacnet/basic/object/device.h"
#include "bacnet/basic/object/ai.h"
#include "bacnet/basic/object/bi.h"
#include "bacnet/basic/object/av.h"
}

static uint8_t Rx_Buf[MAX_MPDU];
static struct mstimer BACnet_Task_Timer;
static struct mstimer BACnet_TSM_Timer;
static struct mstimer BACnet_Address_Timer;

BACnetInterface::BACnetInterface() : running(false) {
}

BACnetInterface::~BACnetInterface() {
    shutdown();
}

bool BACnetInterface::initialize(uint32_t deviceId, const std::string& deviceName) {
    printf("Initializing BACnet device %u (%s)...\n", deviceId, deviceName.c_str());

    // Set device instance number
    Device_Set_Object_Instance_Number(deviceId);

    // Initialize address cache
    address_init();

    // Initialize device and service handlers
    initServiceHandlers();

    // Set device name
    Device_Object_Name_ANSI_Init(deviceName.c_str());

    // Create our objects
    createObjects();

    // Initialize datalink
    dlenv_init();

    // Send I-Am broadcast
    Send_I_Am(&Handler_Transmit_Buffer[0]);

    running = true;
    printf("BACnet device initialized successfully\n");
    return true;
}

void BACnetInterface::initServiceHandlers() {
    Device_Init(NULL);

    // Set up service handlers
    apdu_set_unconfirmed_handler(SERVICE_UNCONFIRMED_WHO_IS, handler_who_is);
    apdu_set_unconfirmed_handler(SERVICE_UNCONFIRMED_WHO_HAS, handler_who_has);
    apdu_set_unrecognized_service_handler_handler(handler_unrecognized_service);

    apdu_set_confirmed_handler(SERVICE_CONFIRMED_READ_PROPERTY, handler_read_property);
    apdu_set_confirmed_handler(SERVICE_CONFIRMED_READ_PROP_MULTIPLE, handler_read_property_multiple);
    apdu_set_confirmed_handler(SERVICE_CONFIRMED_WRITE_PROPERTY, handler_write_property);
    apdu_set_confirmed_handler(SERVICE_CONFIRMED_SUBSCRIBE_COV, handler_cov_subscribe);
    apdu_set_confirmed_handler(SERVICE_CONFIRMED_DEVICE_COMMUNICATION_CONTROL,
                               handler_device_communication_control);

    // Initialize timers
    mstimer_set(&BACnet_Task_Timer, 1000UL);
    mstimer_set(&BACnet_TSM_Timer, 50UL);
    mstimer_set(&BACnet_Address_Timer, 60000UL);
}

void BACnetInterface::createObjects() {
    const auto& sensors = Config::instance().getSensors();

    // Create Analog Inputs for sensors
    for (size_t i = 0; i < sensors.size(); i++) {
        uint32_t instance = static_cast<uint32_t>(i);
        Analog_Input_Create(instance);
        Analog_Input_Name_Set(instance, sensors[i].name.c_str());
        Analog_Input_Present_Value_Set(instance, 0.0f);

        // Set units based on sensor unit string
        if (sensors[i].unit == "°C") {
            Analog_Input_Units_Set(instance, UNITS_DEGREES_CELSIUS);
        } else if (sensors[i].unit == "L/min") {
            Analog_Input_Units_Set(instance, UNITS_LITERS_PER_MINUTE);
        } else if (sensors[i].unit == "bar") {
            Analog_Input_Units_Set(instance, UNITS_BARS);
        }

        printf("  Created AI:%u - %s\n", instance, sensors[i].name.c_str());
    }

    // Create Binary Inputs for alarms (2 per sensor: HIGH and LOW)
    uint32_t biInstance = 0;
    for (const auto& sensor : sensors) {
        // HIGH alarm
        Binary_Input_Create(biInstance);
        alarmNames.push_back(sensor.name + " HIGH Alarm");
        Binary_Input_Name_Set(biInstance, alarmNames.back().c_str());
        Binary_Input_Present_Value_Set(biInstance, BINARY_INACTIVE);
        printf("  Created BI:%u - %s\n", biInstance, alarmNames.back().c_str());
        biInstance++;

        // LOW alarm
        Binary_Input_Create(biInstance);
        alarmNames.push_back(sensor.name + " LOW Alarm");
        Binary_Input_Name_Set(biInstance, alarmNames.back().c_str());
        Binary_Input_Present_Value_Set(biInstance, BINARY_INACTIVE);
        printf("  Created BI:%u - %s\n", biInstance, alarmNames.back().c_str());
        biInstance++;
    }

    // Create Analog Value for efficiency
    Analog_Value_Create(0);
    Analog_Value_Name_Set(0, "Heat Exchanger Efficiency");
    Analog_Value_Present_Value_Set(0, 0.0f, 16);
    Analog_Value_Units_Set(0, UNITS_PERCENT);
    printf("  Created AV:0 - Heat Exchanger Efficiency\n");
}

void BACnetInterface::process() {
    if (!running) return;

    BACNET_ADDRESS src = {0};
    unsigned timeout = 1;

    // Receive and process BACnet messages
    uint16_t pdu_len = datalink_receive(&src, Rx_Buf, MAX_MPDU, timeout);
    if (pdu_len) {
        npdu_handler(&src, Rx_Buf, pdu_len);
    }

    // Handle timers
    if (mstimer_expired(&BACnet_Task_Timer)) {
        mstimer_reset(&BACnet_Task_Timer);
        uint32_t elapsed_ms = mstimer_interval(&BACnet_Task_Timer);
        uint32_t elapsed_s = elapsed_ms / 1000;
        dcc_timer_seconds(elapsed_s);
        datalink_maintenance_timer(elapsed_s);
        handler_cov_timer_seconds(elapsed_s);
    }

    if (mstimer_expired(&BACnet_TSM_Timer)) {
        mstimer_reset(&BACnet_TSM_Timer);
        tsm_timer_milliseconds(mstimer_interval(&BACnet_TSM_Timer));
    }

    if (mstimer_expired(&BACnet_Address_Timer)) {
        mstimer_reset(&BACnet_Address_Timer);
        address_cache_timer(mstimer_interval(&BACnet_Address_Timer) / 1000);
    }

    handler_cov_task();
}

void BACnetInterface::updateValues(SensorManager& sensors, AlarmManager& alarms) {
    if (!running) return;

    const auto& sensorConfigs = Config::instance().getSensors();
    auto values = sensors.getAllValues();

    // Update Analog Inputs with sensor values
    for (size_t i = 0; i < sensorConfigs.size(); i++) {
        float value = values[sensorConfigs[i].id];
        Analog_Input_Present_Value_Set(static_cast<uint32_t>(i), value);
    }

    // Update Binary Inputs with alarm states
    uint32_t biInstance = 0;
    for (const auto& sensor : sensorConfigs) {
        // HIGH alarm
        bool highActive = alarms.isAlarmActive(sensor.id, "HIGH");
        Binary_Input_Present_Value_Set(biInstance, highActive ? BINARY_ACTIVE : BINARY_INACTIVE);
        biInstance++;

        // LOW alarm
        bool lowActive = alarms.isAlarmActive(sensor.id, "LOW");
        Binary_Input_Present_Value_Set(biInstance, lowActive ? BINARY_ACTIVE : BINARY_INACTIVE);
        biInstance++;
    }

    // Update efficiency
    float efficiency = sensors.calculateEfficiency();
    Analog_Value_Present_Value_Set(0, efficiency, 16);
}

void BACnetInterface::shutdown() {
    if (running) {
        datalink_cleanup();
        running = false;
        printf("BACnet interface shutdown\n");
    }
}
