#
# Copyright (C) 2019 The LineageOS Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

#
# This file sets variables that control the way modules are built
# thorughout the system. It should not be used to conditionally
# disable makefiles (the proper mechanism to control what gets
# included in a build is to use PRODUCT_PACKAGES in a product
# definition file).
#

PLATFORM_PATH := device/motorola/sdm632-common

# Platform
TARGET_ARCH := arm64
TARGET_ARCH_VARIANT := armv8-a
TARGET_CPU_ABI := arm64-v8a
TARGET_CPU_ABI2 :=
TARGET_CPU_VARIANT := generic
TARGET_CPU_VARIANT_RUNTIME := kryo

TARGET_2ND_ARCH := arm
TARGET_2ND_ARCH_VARIANT := armv8-a
TARGET_2ND_CPU_ABI := armeabi-v7a
TARGET_2ND_CPU_ABI2 := armeabi
TARGET_2ND_CPU_VARIANT := generic
TARGET_2ND_CPU_VARIANT_RUNTIME := cortex-a53

BOARD_USES_QCOM_HARDWARE := true
TARGET_BOOTLOADER_BOARD_NAME := SDM632
TARGET_BOARD_PLATFORM := msm8953

# A/B updater
AB_OTA_PARTITIONS += \
    boot \
    dtbo \
    system \
    vendor

# Audio
BOARD_USES_ALSA_AUDIO := true
BOARD_SUPPORTS_SOUND_TRIGGER_HAL := true
DTS_CODEC_M_ := false
MM_AUDIO_ENABLED_FTM := true
MM_AUDIO_ENABLED_SAFX := true
TARGET_USES_QCOM_MM_AUDIO := true

AUDIO_FEATURE_ENABLED_EXTENDED_COMPRESS_FORMAT := true
AUDIO_FEATURE_ENABLED_COMPRESS_CAPTURE := false
AUDIO_FEATURE_ENABLED_COMPRESS_VOIP := true
AUDIO_FEATURE_ENABLED_EXTN_FORMATS := true
AUDIO_FEATURE_ENABLED_EXTN_FLAC_DECODER := true
AUDIO_FEATURE_ENABLED_EXTN_RESAMPLER := false
AUDIO_FEATURE_ENABLED_FM_POWER_OPT := true
AUDIO_FEATURE_ENABLED_HDMI_SPK := true
AUDIO_FEATURE_ENABLED_PCM_OFFLOAD := true
AUDIO_FEATURE_ENABLED_PCM_OFFLOAD_24 := true
AUDIO_FEATURE_ENABLED_FLAC_OFFLOAD := true
AUDIO_FEATURE_ENABLED_VORBIS_OFFLOAD := false
AUDIO_FEATURE_ENABLED_WMA_OFFLOAD := false
AUDIO_FEATURE_ENABLED_ALAC_OFFLOAD := false
AUDIO_FEATURE_ENABLED_APE_OFFLOAD := true
AUDIO_FEATURE_ENABLED_AAC_ADTS_OFFLOAD := true
AUDIO_FEATURE_ENABLED_PROXY_DEVICE := true
AUDIO_FEATURE_ENABLED_SSR := false
AUDIO_FEATURE_ENABLED_DTS_EAGLE := false
AUDIO_FEATURE_ENABLED_HW_ACCELERATED_EFFECTS := false
AUDIO_FEATURE_ENABLED_AUDIOSPHERE := true
AUDIO_FEATURE_ENABLED_USB_TUNNEL_AUDIO := true
AUDIO_FEATURE_ENABLED_SPLIT_A2DP := false
AUDIO_FEATURE_ENABLED_3D_AUDIO := true
AUDIO_FEATURE_ENABLED_VBAT_MONITOR := true
AUDIO_FEATURE_ENABLED_ANC_HEADSET := true
AUDIO_FEATURE_ENABLED_CUSTOMSTEREO := true
AUDIO_FEATURE_ENABLED_FLUENCE := true
AUDIO_FEATURE_ENABLED_HDMI_EDID := true
AUDIO_FEATURE_ENABLED_HDMI_PASSTHROUGH := true
AUDIO_FEATURE_ENABLED_DISPLAY_PORT := true
AUDIO_FEATURE_ENABLED_DS2_DOLBY_DAP := true
AUDIO_FEATURE_ENABLED_HFP := true
AUDIO_FEATURE_ENABLED_INCALL_MUSIC := true
AUDIO_FEATURE_ENABLED_MULTI_VOICE_SESSIONS := true
AUDIO_FEATURE_ENABLED_SPKR_PROTECTION := true
AUDIO_FEATURE_ENABLED_ACDB_LICENSE := true
AUDIO_FEATURE_ENABLED_DEV_ARBI := false
AUDIO_FEATURE_ENABLED_SOURCE_TRACKING := true
AUDIO_FEATURE_ENABLED_GEF_SUPPORT := true
AUDIO_FEATURE_ENABLED_RAS := true
AUDIO_FEATURE_ENABLED_PERF_HINTS := true
AUDIO_USE_LL_AS_PRIMARY_OUTPUT := true

# Bluetooth
BOARD_HAVE_BLUETOOTH_QCOM := true

# Camera
USE_CAMERA_STUB := true

# HIDL
DEVICE_FRAMEWORK_COMPATIBILITY_MATRIX_FILE := \
    $(PLATFORM_PATH)/framework_compatibility_matrix.xml \
    hardware/qcom-caf/common/vendor_framework_compatibility_matrix.xml \
    hardware/qcom-caf/common/vendor_framework_compatibility_matrix_legacy.xml
# vendor/lineage/config/device_framework_matrix.xml dropped — lineage-23/A16 removed this
# explicit reference (vendor/lineage injects its own framework matrix); the file no longer
# exists at that path. Matches the A16 reference fix (sm6225-common a83f1ad).
DEVICE_MANIFEST_FILE := $(PLATFORM_PATH)/manifest.xml
DEVICE_MATRIX_FILE := hardware/qcom-caf/common/compatibility_matrix.xml
TARGET_FS_CONFIG_GEN += \
    $(PLATFORM_PATH)/config.fs \
    $(PLATFORM_PATH)/mot_aids.fs

# Kernel
BOARD_KERNEL_CMDLINE += androidboot.hardware=qcom ehci-hcd.park=3 lpm_levels.sleep_disabled=1
BOARD_KERNEL_CMDLINE += androidboot.bootdevice=7824900.sdhci androidboot.usbconfigfs=true
BOARD_KERNEL_CMDLINE += loop.max_part=7 androidboot.boot_devices=soc/7824900.sdhci
BOARD_KERNEL_CMDLINE += androidboot.veritymode=eio
# === DEBUG (remove after first-boot bring-up) ===
# watchdog_v2.enable=0: disable the MSM HW watchdog (watchdog_v2.c module_param) so a kernel
#   hang during #38's boot doesn't silently reset at ~5s with no log (and no charger-loop
#   pstore overwrite). With it off, #38 either boots through to adbd, or PANICs at the real
#   failure point — which writes a dmesg-ramoops dump WITH the #38 banner + reason we can read.
# androidboot.selinux=permissive: rule out an SELinux-denial-induced init reboot in the same pass.
BOARD_KERNEL_CMDLINE += watchdog_v2.enable=0
BOARD_KERNEL_CMDLINE += androidboot.selinux=permissive
# recovery-as-boot: this legacy Moto bootloader does NOT inject androidboot.force_normal_boot, so
# first-stage init otherwise stays in recovery -> bootloop. Hardcode it (the known-good hand-packed
# boot.img carried it). Verified via boot-header diff of the plain `m bootimage` output.
BOARD_KERNEL_CMDLINE += androidboot.force_normal_boot=1
# printk.devkmsg=on: disable the /dev/kmsg write ratelimit so netbpfload's full BTF
# verifier log reaches the kernel console (ramoops). Default "ratelimit" suppressed
# 313 lines of the BTF_LOAD -22 dump (2026-07-05). DIAG aid; drop for ship.
BOARD_KERNEL_CMDLINE += printk.devkmsg=on
BOARD_KERNEL_BASE := 0x80000000
BOARD_KERNEL_PAGESIZE :=  2048
BOARD_KERNEL_OFFSET := 0x00008000
BOARD_KERNEL_IMAGE_NAME := Image.gz-dtb
BOARD_KERNEL_SEPARATED_DTBO := true
BOARD_INCLUDE_RECOVERY_DTBO := true
TARGET_KERNEL_SOURCE := kernel/motorola/sdm632
TARGET_KERNEL_VERSION := 4.9
# r584948 is soong's ClangDefaultVersion, i.e. the clang AOSP already ships in prebuilts, so nothing is
# fetched out of band. This still has to be named explicitly rather than left to BoardConfigKernel.mk's
# default: that default is $(LLVM_AOSP_PREBUILTS_VERSION), which vendor/lineage/build/envsetup.sh exports
# from ${CLANG_VERSION}, and nothing sets CLANG_VERSION in this tree -- it resolves empty, leaving
# TARGET_KERNEL_CLANG_PATH pointing at a versionless directory.
#
# This was previously pinned to r536225 (fetched out of band, ~4.7 GB) on the belief that newer clang
# miscompiles this kernel into a fault before console init. That was wrong: newer clang does not
# miscompile the kernel, it declines to compile it. -Wdefault-const-init-* and -Wimplicit-enum-enum-cast,
# neither of which existed in clang 19, reject five genuine bugs in the QCOM techpack/audio vendor code.
# With those fixed the kernel builds and boots with BTF, eBPF and Mesa intact.
#
# Must be set before BoardConfigKernel.mk (tail of channel/BoardConfig.mk) reads KERNEL_CLANG_VERSION /
# TARGET_KERNEL_CLANG_PATH.
TARGET_KERNEL_CLANG_VERSION := r584948

# CONFIG_DEBUG_INFO_BTF needs pahole. BoardConfigKernel.mk overrides PAHOLE= with an absolute path into
# prebuilts/kernel-build-tools, which no platform manifest syncs, so the kernel links with an empty .BTF
# section and the BPF programs that need CO-RE fail at load with -EINVAL. Point it at the host's pahole
# (the dwarves package) instead; kernel.mk appends these flags after BoardConfigKernel.mk, so this
# assignment wins. Resolve it against a clean PATH rather than the build's: soong replaces PATH with a
# sandbox of wrappers that refuse to exec pahole ("not allowed to be used"), so both a bare name and a
# $(shell command -v pahole) would find the wrapper. Only an absolute path to the real binary escapes it.
TARGET_KERNEL_ADDITIONAL_FLAGS := PAHOLE=$(shell PATH=/usr/local/bin:/usr/bin:/bin command -v pahole)

# Declare boot header
BOARD_BOOT_HEADER_VERSION := 1
BOARD_MKBOOTIMG_ARGS += --header_version $(BOARD_BOOT_HEADER_VERSION)
# ramdisk_offset 0x03800000 (load addr 0x83800000): the ramdisk must load above the kernel image, which
# with CONFIG_DEBUG_INFO_BTF reaches ~0x8322c000. mkbootimg's default 0x01000000, and the 0x03000000 that
# sufficed before BTF, both land inside kernel BSS -> the kernel silently resets a few seconds in.
BOARD_MKBOOTIMG_ARGS += --ramdisk_offset 0x03800000

# Lights
TARGET_PROVIDES_LIBLIGHT := true

# Media
TARGET_USES_ION := true

# Mesa3D (aospext) — build the freedreno gallium driver on KGSL natively via
# soong so libgallium_dri / libEGL_mesa / libGLES*_mesa / libgbm_mesa are
# produced in-tree (replaces the prebuilt adreno EGL blobs). Requires the
# external/aospext/Android.mk allowlist entry (vendor/google/build/androidmk/
# allowlist.txt) so the AOSP androidmk denylist does not fatal the build.
BOARD_BUILD_AOSPEXT_MESA3D := true
BOARD_MESA3D_SRC_DIR       := external/mesa3d
BOARD_MESA3D_GALLIUM_DRIVERS := freedreno
BOARD_MESA3D_VULKAN_DRIVERS  :=
BOARD_MESA3D_BUILD_LIBGBM    := true
BOARD_MESA3D_EXTRA_MESON_ARGS := -Dfreedreno-kmds=kgsl

# NFC / ODM
ODM_MANIFEST_SKUS += nfc
ODM_MANIFEST_NFC_FILES := $(PLATFORM_PATH)/odm_manifest_nfc.xml

# Partitions
BOARD_FLASH_BLOCK_SIZE := 131072                  # (BOARD_KERNEL_PAGESIZE * 64)
BOARD_DTBOIMG_PARTITION_SIZE := 8388608
# REAL vendor partition = 320MB (fastboot getvar partition-size:vendor_b = 0x14000000 =
# 335544320), NOT ~821MB as an earlier wrong comment claimed. Uncompressed /vendor is ~356MB
# so it does NOT fit ext4 in 320MB (flash rejected: "Image size exceeded partition limits").
# Stock used squashfs/gzip (~140-180MB compressed, fits). BUT AOSP-17's soong "fsgen" can't
# build a squashfs vendor (analysis error: generated recovery-prop depends on undefined
# vendor-build.prop). So fs type stays ext4 HERE for the build to analyze/compile, and the
# FLASHABLE squashfs vendor is produced out-of-band via system/extras/squashfs_utils/
# mksquashfsimage.sh on out/target/product/channel/vendor (labels from vendor_file_contexts).
# TODO: fix soong fsgen squashfs vendor so `m droid` emits a flashable image directly.
# Vendor FS = EROFS (read-only, lz4hc-9 compressed). The kernel has the erofs
# backport (CONFIG_EROFS_FS=y + ZIP/xattr/acl/security) so soong emits a native
# erofs vendor image that fits the real 320MB partition (was the ext4/squashfs
# out-of-band hack). Flash the erofs-capable boot.img BEFORE the erofs vendor.
BOARD_VENDORIMAGE_FILE_SYSTEM_TYPE := erofs
BOARD_EROFS_COMPRESSOR             := lz4hc,9
BOARD_VENDORIMAGE_JOURNAL_SIZE     := 0
BOARD_USES_RECOVERY_AS_BOOT := true
TARGET_NO_RECOVERY := true
TARGET_USERIMAGES_USE_EXT4 := true
TARGET_USERIMAGES_USE_F2FS := true
TARGET_COPY_OUT_VENDOR := vendor

BOARD_ROOT_EXTRA_SYMLINKS := \
    /vendor/fsg:/fsg \
    /mnt/vendor/persist:/persist

# Metadata partition (mmcblk0p39). A17 aconfigd/apexd require /metadata mounted;
# LOS(A15) left it unmounted. This declares it so the /metadata mountpoint is
# created in the root and recovery wipes it on factory reset.
BOARD_USES_METADATA_PARTITION := true

# Power
TARGET_HAS_NO_WLAN_STATS := true
TARGET_USES_INTERACTION_BOOST := true

# Properties
TARGET_ODM_PROP += $(PLATFORM_PATH)/odm.prop
TARGET_PRODUCT_PROP += $(PLATFORM_PATH)/product.prop
TARGET_SYSTEM_PROP += $(PLATFORM_PATH)/system.prop
TARGET_VENDOR_PROP += $(PLATFORM_PATH)/vendor.prop

# RIL
ENABLE_VENDOR_RIL_SERVICE := true

# Recovery
TARGET_RECOVERY_FSTAB := $(PLATFORM_PATH)/rootdir/etc/fstab.qcom

# Root
# /persist is now a compat symlink to the rw /mnt/vendor/persist mount (see
# BOARD_ROOT_EXTRA_SYMLINKS above), so legacy init /persist/* writes no longer
# hit the read-only rootfs. Do not recreate it as a bare folder.
BOARD_ROOT_EXTRA_FOLDERS :=

# Vendor Security Patch Level
VENDOR_SECURITY_PATCH := 2021-02-01

# SELinux
include device/qcom/sepolicy-legacy-um/SEPolicy.mk
BOARD_VENDOR_SEPOLICY_DIRS += $(PLATFORM_PATH)/sepolicy/vendor
# moved to common.mk (PRODUCT_* readonly in AOSP BoardConfig): PRODUCT_PRIVATE_SEPOLICY_DIRS

# Verified Boot
BOARD_AVB_ENABLE := false
BOARD_AVB_MAKE_VBMETA_IMAGE_ARGS += --flags 3

# Wifi
# AOSP-17 ships hardware/qcom/wlan (base manifest) whose legacy/qcwcn Android.mk
# redefines lib_driver_cmd_qcwcn / wcnss_service / wpa_supplicant.conf — colliding
# with the grafted (device-correct, blob-matching) hardware/qcom-caf/wlan tree that
# already provides those in the default namespace. Disable the AOSP qcom/wlan tree.
TARGET_USES_HARDWARE_QCOM_WLAN := false

BOARD_WLAN_DEVICE := qcwcn
WPA_SUPPLICANT_VERSION := VER_0_8_X
BOARD_WPA_SUPPLICANT_DRIVER := NL80211
BOARD_WPA_SUPPLICANT_PRIVATE_LIB := lib_driver_cmd_$(BOARD_WLAN_DEVICE)
BOARD_HOSTAPD_DRIVER := NL80211
BOARD_HOSTAPD_PRIVATE_LIB := lib_driver_cmd_$(BOARD_WLAN_DEVICE)
WIFI_DRIVER_FW_PATH_STA := "sta"
WIFI_DRIVER_FW_PATH_AP  := "ap"
WIFI_DRIVER_FW_PATH_P2P := "p2p"
WIFI_HIDL_UNIFIED_SUPPLICANT_SERVICE_RC_ENTRY := true
# moved to common.mk (PRODUCT_* readonly in AOSP BoardConfig): PRODUCT_VENDOR_MOVE_ENABLED := true

# The LineageOS build-manifest.xml task runs `repo manifest`, which writes config/trace/
# lock files under .repo (.repo/TRACE_FILE, .repo/manifests.git/.repo_config.json, ...).
# AOSP-17 wraps the whole ninja run in an nsjail that mounts the source tree READ-ONLY
# (out/ is RW), so repo's writes fail with "Read-only file system". Allowlist .repo as RW
# in that sandbox so `repo manifest` can run. (REPO_TRACE=0 is also set on the recipe to
# avoid the 34MB trace file growth.)
# NB: must be ABSOLUTE — sandbox_linux.go passes each entry verbatim to nsjail `-B`,
# and nsjail resolves a relative src against its own cwd (fails "No such file or directory").
BUILD_BROKEN_SRC_DIR_RW_ALLOWLIST := $(abspath .repo)

# Prebuilt vendor blobs netmgrd/adpl (Android-11 era) link librmnetctl and need the OLD
# rmnet API (rmnet_associate_network_device, rmnet_new_vnd, rmnetctl_init, ...), but the
# tree builds librmnetctl from source (vendor/qcom/.../dataservices/rmnetctl) with a NEWER
# API -> soong check_elf_file fails "Unresolved symbol: rmnet_*". Disable the prebuilt ELF
# symbol check (build/make/core/check_elf_file.mk + soong BuildBrokenPrebuiltELFFiles) so
# vendor.img can build. (Bring-up escape hatch; rmnet-ABI runtime correctness for mobile
# data is a later fix — graft the matching prebuilt librmnetctl or rebuild the blobs.)
BUILD_BROKEN_PREBUILT_ELF_FILES := true

# ABI FIX (rmnet): the OLD rmnet ioctl API (rmnetctl_init, rmnet_new_vnd,
# rmnet_set_link_ingress_data_format_tailspace, rmnet_associate_network_device, ...) that
# the A10 netmgrd/adpl blobs import lives inside #ifdef USE_OLD_RMNET_DATA in
# vendor/qcom/opensource/dataservices/rmnetctl/src/librmnetctl.c. That block is gated by
# soong_config rmnetctl.old_rmnet_data, which upstream sets in hardware/qcom-caf/common/
# BoardConfigQcom.mk (NOT inherited by this board), so librmnetctl.so shipped only the NEW
# rtrmnet_* netlink API and the blobs failed dlopen at runtime. Enable it here: this board's
# kernel is 4.9 (old rmnet_data driver) and the required uapi header linux/rmnet_data.h IS
# present in the generated kernel headers (qti_kernel_headers). Adds the 12 legacy symbols
# to librmnetctl.so while keeping the new rtrmnet_* API (both are compiled; old block is
# additive, ends before the rtrmnet_* definitions).
$(call soong_config_set,rmnetctl,old_rmnet_data,true)
