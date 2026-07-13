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

#define LOG_TAG "thermal-service.sdm632"

#include "Thermal.h"

#include <android-base/file.h>
#include <android-base/logging.h>
#include <android-base/parseint.h>
#include <android-base/strings.h>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <dirent.h>
#include <set>

namespace aidl {
namespace android {
namespace hardware {
namespace thermal {
namespace impl {
namespace sdm632 {

using ::android::base::ReadFileToString;
using ::android::base::StartsWith;
using ::android::base::Trim;

namespace {

constexpr char kThermalSysfs[] = "/sys/class/thermal";
constexpr std::chrono::seconds kPollInterval{2};
// ThrottlingSeverity cardinality, per TemperatureThreshold's contract.
constexpr size_t kSeverityCount = 7;

bool readSysfsString(const std::string& path, std::string* out) {
    std::string content;
    if (!ReadFileToString(path, &content)) return false;
    *out = Trim(content);
    return true;
}

bool readSysfsInt(const std::string& path, int64_t* out) {
    std::string content;
    if (!readSysfsString(path, &content)) return false;
    return ::android::base::ParseInt(content, out);
}

bool contains(const std::string& haystack, const char* needle) {
    return haystack.find(needle) != std::string::npos;
}

// The kernel exposes no type taxonomy, only a free-form zone name, so classify
// by the naming conventions msm8953's tsens/board-thermistor zones use. Zone
// names mix '-' and '_' as separators (xo-therm-adc vs msm_therm), so match
// against a separator-normalized form.
TemperatureType classifyZone(const std::string& raw_name) {
    std::string name = raw_name;
    std::replace(name.begin(), name.end(), '-', '_');

    if (contains(name, "gpu")) return TemperatureType::GPU;
    if (contains(name, "cpu") || contains(name, "cluster") || contains(name, "kryo") ||
        contains(name, "apc"))
        return TemperatureType::CPU;
    if (contains(name, "batt") || contains(name, "bms") || contains(name, "bcl")) {
        return TemperatureType::BATTERY;
    }
    if (contains(name, "usb") || contains(name, "conn")) return TemperatureType::USB_PORT;
    if (contains(name, "modem") || contains(name, "mdm")) return TemperatureType::MODEM;
    if (contains(name, "camera") || contains(name, "cam")) return TemperatureType::CAMERA;
    if (contains(name, "flash")) return TemperatureType::FLASHLIGHT;
    if (contains(name, "pa_therm")) return TemperatureType::POWER_AMPLIFIER;
    // Board-mounted thermistors are what the framework's skin-temperature
    // consumers (thermal headroom, PowerManager) actually want.
    if (contains(name, "skin") || contains(name, "quiet") || contains(name, "case") ||
        contains(name, "xo_therm") || contains(name, "msm_therm") ||
        contains(name, "emmc_therm") || contains(name, "chg_therm") ||
        contains(name, "pm_therm")) {
        return TemperatureType::SKIN;
    }
    // The zone named "soc" is msm8953's BCL state-of-charge reading -- a battery
    // percentage, not a temperature -- so it must never be typed as SOC.
    if (contains(name, "tsens")) return TemperatureType::SOC;
    return TemperatureType::UNKNOWN;
}

CoolingType classifyCoolingDevice(const std::string& name) {
    if (contains(name, "gpu")) return CoolingType::GPU;
    if (contains(name, "cpu") || contains(name, "thermal-cpufreq") || contains(name, "cluster")) {
        return CoolingType::CPU;
    }
    if (contains(name, "battery") || contains(name, "bcl") || contains(name, "charger")) {
        return CoolingType::BATTERY;
    }
    if (contains(name, "modem") || contains(name, "mdm")) return CoolingType::MODEM;
    if (contains(name, "display") || contains(name, "lcd")) return CoolingType::DISPLAY;
    if (contains(name, "fan")) return CoolingType::FAN;
    return CoolingType::COMPONENT;
}

// Maps a trip point's kernel type to the severity it should raise.
std::optional<ThrottlingSeverity> severityForTrip(const std::string& trip_type) {
    if (trip_type == "active") return ThrottlingSeverity::LIGHT;
    if (trip_type == "passive") return ThrottlingSeverity::MODERATE;
    if (trip_type == "hot") return ThrottlingSeverity::SEVERE;
    if (trip_type == "critical") return ThrottlingSeverity::SHUTDOWN;
    return std::nullopt;
}

// Reads trip_point_N_{type,temp} into a ThrottlingSeverity-indexed threshold array.
std::vector<float> readThresholds(const std::string& zone_path) {
    std::vector<float> thresholds(kSeverityCount, NAN);

    for (int i = 0;; i++) {
        const std::string prefix = zone_path + "/trip_point_" + std::to_string(i);
        std::string trip_type;
        int64_t trip_temp;
        if (!readSysfsString(prefix + "_type", &trip_type)) break;
        if (!readSysfsInt(prefix + "_temp", &trip_temp)) continue;

        const auto severity = severityForTrip(trip_type);
        if (!severity) continue;

        const float celsius = static_cast<float>(trip_temp) / 1000.0f;
        const size_t index = static_cast<size_t>(*severity);
        // A zone may declare several trips of one kind; the lowest is the one
        // that first raises this severity.
        if (std::isnan(thresholds[index]) || celsius < thresholds[index]) {
            thresholds[index] = celsius;
        }
    }

    return thresholds;
}

ThrottlingSeverity severityFor(float celsius, const std::vector<float>& thresholds) {
    ThrottlingSeverity severity = ThrottlingSeverity::NONE;
    for (size_t i = 1; i < thresholds.size(); i++) {
        if (!std::isnan(thresholds[i]) && celsius >= thresholds[i]) {
            severity = static_cast<ThrottlingSeverity>(i);
        }
    }
    return severity;
}

}  // namespace

Thermal::Thermal() {
    std::unique_ptr<DIR, decltype(&closedir)> dir(opendir(kThermalSysfs), closedir);
    if (dir == nullptr) {
        PLOG(ERROR) << "Cannot open " << kThermalSysfs;
        return;
    }

    std::set<std::string> used_names;
    while (struct dirent* entry = readdir(dir.get())) {
        const std::string entry_name(entry->d_name);
        const bool is_zone = StartsWith(entry_name, "thermal_zone");
        const bool is_cdev = StartsWith(entry_name, "cooling_device");
        if (!is_zone && !is_cdev) continue;

        const std::string path = std::string(kThermalSysfs) + "/" + entry_name;
        std::string name;
        if (!readSysfsString(path + "/type", &name) || name.empty()) {
            LOG(WARNING) << "Skipping " << entry_name << ": no type";
            continue;
        }

        // Temperature and CoolingDevice both require names to be unique within
        // a type; some msm8953 zones share a kernel name.
        std::string unique_name = name;
        for (int suffix = 1; used_names.count(unique_name) != 0; suffix++) {
            unique_name = name + "-" + std::to_string(suffix);
        }
        used_names.insert(unique_name);

        if (is_zone) {
            zones_.push_back(ThermalZone{
                    .path = path,
                    .name = unique_name,
                    .type = classifyZone(name),
                    .hot_thresholds = readThresholds(path),
            });
        } else {
            cooling_zones_.push_back(CoolingZone{
                    .path = path,
                    .name = unique_name,
                    .type = classifyCoolingDevice(name),
            });
        }
    }

    LOG(INFO) << "Found " << zones_.size() << " thermal zones, " << cooling_zones_.size()
              << " cooling devices";

    poll_thread_ = std::thread(&Thermal::pollLoop, this);
}

Thermal::~Thermal() {
    {
        std::lock_guard<std::mutex> lock(stop_mutex_);
        stop_ = true;
    }
    stop_cv_.notify_all();
    if (poll_thread_.joinable()) poll_thread_.join();
}

bool Thermal::readZone(ThermalZone* zone, Temperature* out) {
    int64_t millicelsius;
    if (!readSysfsInt(zone->path + "/temp", &millicelsius)) return false;

    const float celsius = static_cast<float>(millicelsius) / 1000.0f;
    zone->severity = severityFor(celsius, zone->hot_thresholds);

    *out = Temperature{
            .type = zone->type,
            .name = zone->name,
            .value = celsius,
            .throttlingStatus = zone->severity,
    };
    return true;
}

ndk::ScopedAStatus Thermal::getTemperaturesFiltered(std::optional<TemperatureType> filter,
                                                    std::vector<Temperature>* out) {
    std::lock_guard<std::mutex> lock(zones_mutex_);
    for (auto& zone : zones_) {
        if (filter && zone.type != *filter) continue;

        Temperature temperature;
        if (readZone(&zone, &temperature)) out->push_back(temperature);
    }
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Thermal::getTemperatures(std::vector<Temperature>* out_temperatures) {
    return getTemperaturesFiltered(std::nullopt, out_temperatures);
}

ndk::ScopedAStatus Thermal::getTemperaturesWithType(TemperatureType in_type,
                                                    std::vector<Temperature>* out_temperatures) {
    return getTemperaturesFiltered(in_type, out_temperatures);
}

ndk::ScopedAStatus Thermal::getThresholdsFiltered(std::optional<TemperatureType> filter,
                                                  std::vector<TemperatureThreshold>* out) {
    std::lock_guard<std::mutex> lock(zones_mutex_);
    for (const auto& zone : zones_) {
        if (filter && zone.type != *filter) continue;

        out->push_back(TemperatureThreshold{
                .type = zone.type,
                .name = zone.name,
                .hotThrottlingThresholds = zone.hot_thresholds,
                .coldThrottlingThresholds = std::vector<float>(kSeverityCount, NAN),
        });
    }
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Thermal::getTemperatureThresholds(
        std::vector<TemperatureThreshold>* out_thresholds) {
    return getThresholdsFiltered(std::nullopt, out_thresholds);
}

ndk::ScopedAStatus Thermal::getTemperatureThresholdsWithType(
        TemperatureType in_type, std::vector<TemperatureThreshold>* out_thresholds) {
    return getThresholdsFiltered(in_type, out_thresholds);
}

ndk::ScopedAStatus Thermal::getCoolingDevices(std::vector<CoolingDevice>* out_devices) {
    std::lock_guard<std::mutex> lock(zones_mutex_);
    for (const auto& cdev : cooling_zones_) {
        int64_t value;
        if (!readSysfsInt(cdev.path + "/cur_state", &value)) continue;

        out_devices->push_back(CoolingDevice{
                .type = cdev.type,
                .name = cdev.name,
                .value = value,
        });
    }
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Thermal::getCoolingDevicesWithType(CoolingType in_type,
                                                      std::vector<CoolingDevice>* out_devices) {
    std::vector<CoolingDevice> devices;
    auto status = getCoolingDevices(&devices);
    if (!status.isOk()) return status;

    std::copy_if(devices.begin(), devices.end(), std::back_inserter(*out_devices),
                 [in_type](const CoolingDevice& device) { return device.type == in_type; });
    return ndk::ScopedAStatus::ok();
}

void Thermal::notify(const Temperature& temperature) {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    for (const auto& entry : callbacks_) {
        if (entry.type && *entry.type != temperature.type) continue;
        entry.callback->notifyThrottling(temperature);
    }
}

void Thermal::pollLoop() {
    while (!stop_) {
        {
            std::lock_guard<std::mutex> lock(zones_mutex_);
            for (auto& zone : zones_) {
                const ThrottlingSeverity previous = zone.severity;

                Temperature temperature;
                if (!readZone(&zone, &temperature)) continue;
                if (zone.severity == previous) continue;

                LOG(INFO) << "Zone " << zone.name << " severity "
                          << static_cast<int>(previous) << " -> "
                          << static_cast<int>(zone.severity) << " at " << temperature.value << "C";
                notify(temperature);
            }
        }

        std::unique_lock<std::mutex> lock(stop_mutex_);
        stop_cv_.wait_for(lock, kPollInterval, [this] { return stop_.load(); });
    }
}

ndk::ScopedAStatus Thermal::registerThermalChangedCallback(
        const std::shared_ptr<IThermalChangedCallback>& in_callback) {
    if (in_callback == nullptr) {
        return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
    }

    std::lock_guard<std::mutex> lock(callback_mutex_);
    for (const auto& entry : callbacks_) {
        if (entry.callback->asBinder() == in_callback->asBinder()) {
            return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
        }
    }

    callbacks_.push_back(CallbackEntry{.callback = in_callback, .type = std::nullopt});
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Thermal::registerThermalChangedCallbackWithType(
        const std::shared_ptr<IThermalChangedCallback>& in_callback, TemperatureType in_type) {
    if (in_callback == nullptr) {
        return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
    }

    std::lock_guard<std::mutex> lock(callback_mutex_);
    for (const auto& entry : callbacks_) {
        if (entry.callback->asBinder() == in_callback->asBinder()) {
            return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
        }
    }

    callbacks_.push_back(CallbackEntry{.callback = in_callback, .type = in_type});
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Thermal::unregisterThermalChangedCallback(
        const std::shared_ptr<IThermalChangedCallback>& in_callback) {
    if (in_callback == nullptr) {
        return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
    }

    std::lock_guard<std::mutex> lock(callback_mutex_);
    const auto it = std::remove_if(callbacks_.begin(), callbacks_.end(),
                                   [&in_callback](const CallbackEntry& entry) {
                                       return entry.callback->asBinder() == in_callback->asBinder();
                                   });
    if (it == callbacks_.end()) {
        return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
    }

    callbacks_.erase(it, callbacks_.end());
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Thermal::registerCoolingDeviceChangedCallbackWithType(
        const std::shared_ptr<ICoolingDeviceChangedCallback>& in_callback, CoolingType /*type*/) {
    if (in_callback == nullptr) {
        return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
    }

    std::lock_guard<std::mutex> lock(callback_mutex_);
    cdev_callbacks_.push_back(in_callback);
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Thermal::unregisterCoolingDeviceChangedCallback(
        const std::shared_ptr<ICoolingDeviceChangedCallback>& in_callback) {
    if (in_callback == nullptr) {
        return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
    }

    std::lock_guard<std::mutex> lock(callback_mutex_);
    const auto it = std::remove_if(
            cdev_callbacks_.begin(), cdev_callbacks_.end(),
            [&in_callback](const std::shared_ptr<ICoolingDeviceChangedCallback>& callback) {
                return callback->asBinder() == in_callback->asBinder();
            });
    if (it == cdev_callbacks_.end()) {
        return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
    }

    cdev_callbacks_.erase(it, cdev_callbacks_.end());
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Thermal::forecastSkinTemperature(int32_t /*in_forecastSeconds*/,
                                                    float* /*_aidl_return*/) {
    return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
}

}  // namespace sdm632
}  // namespace impl
}  // namespace thermal
}  // namespace hardware
}  // namespace android
}  // namespace aidl
