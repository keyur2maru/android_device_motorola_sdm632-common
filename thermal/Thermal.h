/*
 * Copyright (C) 2026 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include <aidl/android/hardware/thermal/BnThermal.h>

#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

namespace aidl {
namespace android {
namespace hardware {
namespace thermal {
namespace impl {
namespace sdm632 {

// One /sys/class/thermal/thermal_zoneN, resolved once at start-up.
struct ThermalZone {
    std::string path;
    std::string name;
    TemperatureType type;
    // Indexed by ThrottlingSeverity; NAN where the zone declares no matching trip point.
    std::vector<float> hot_thresholds;
    ThrottlingSeverity severity = ThrottlingSeverity::NONE;
};

// One /sys/class/thermal/cooling_deviceN.
struct CoolingZone {
    std::string path;
    std::string name;
    CoolingType type;
};

class Thermal : public BnThermal {
  public:
    Thermal();
    ~Thermal();

    ndk::ScopedAStatus getCoolingDevices(std::vector<CoolingDevice>* out_devices) override;
    ndk::ScopedAStatus getCoolingDevicesWithType(CoolingType in_type,
                                                 std::vector<CoolingDevice>* out_devices) override;

    ndk::ScopedAStatus getTemperatures(std::vector<Temperature>* out_temperatures) override;
    ndk::ScopedAStatus getTemperaturesWithType(TemperatureType in_type,
                                               std::vector<Temperature>* out_temperatures) override;

    ndk::ScopedAStatus getTemperatureThresholds(
            std::vector<TemperatureThreshold>* out_thresholds) override;
    ndk::ScopedAStatus getTemperatureThresholdsWithType(
            TemperatureType in_type, std::vector<TemperatureThreshold>* out_thresholds) override;

    ndk::ScopedAStatus registerThermalChangedCallback(
            const std::shared_ptr<IThermalChangedCallback>& in_callback) override;
    ndk::ScopedAStatus registerThermalChangedCallbackWithType(
            const std::shared_ptr<IThermalChangedCallback>& in_callback,
            TemperatureType in_type) override;
    ndk::ScopedAStatus unregisterThermalChangedCallback(
            const std::shared_ptr<IThermalChangedCallback>& in_callback) override;

    ndk::ScopedAStatus registerCoolingDeviceChangedCallbackWithType(
            const std::shared_ptr<ICoolingDeviceChangedCallback>& in_callback,
            CoolingType in_type) override;
    ndk::ScopedAStatus unregisterCoolingDeviceChangedCallback(
            const std::shared_ptr<ICoolingDeviceChangedCallback>& in_callback) override;

    ndk::ScopedAStatus forecastSkinTemperature(int32_t in_forecastSeconds,
                                               float* _aidl_return) override;

  private:
    struct CallbackEntry {
        std::shared_ptr<IThermalChangedCallback> callback;
        // Unset means "every type".
        std::optional<TemperatureType> type;
    };

    // Reads the zone's current temperature and updates its cached severity.
    // Returns false when the zone cannot be read (several msm8953 zones return
    // -ENXIO until their sensor is powered).
    bool readZone(ThermalZone* zone, Temperature* out);

    // An unset filter means "every zone", which is what the unfiltered
    // getTemperatures/getTemperatureThresholds ask for.
    ndk::ScopedAStatus getTemperaturesFiltered(std::optional<TemperatureType> filter,
                                               std::vector<Temperature>* out);
    ndk::ScopedAStatus getThresholdsFiltered(std::optional<TemperatureType> filter,
                                             std::vector<TemperatureThreshold>* out);

    void pollLoop();
    void notify(const Temperature& temperature);

    std::vector<ThermalZone> zones_;
    std::vector<CoolingZone> cooling_zones_;
    std::mutex zones_mutex_;

    std::mutex callback_mutex_;
    std::vector<CallbackEntry> callbacks_;
    std::vector<std::shared_ptr<ICoolingDeviceChangedCallback>> cdev_callbacks_;

    std::atomic<bool> stop_{false};
    std::condition_variable stop_cv_;
    std::mutex stop_mutex_;
    std::thread poll_thread_;
};

}  // namespace sdm632
}  // namespace impl
}  // namespace thermal
}  // namespace hardware
}  // namespace android
}  // namespace aidl
