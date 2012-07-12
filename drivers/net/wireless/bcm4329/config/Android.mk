#
# Copyright (C) 2009 Broadcom Corporation
#
# $Id: Android.mk,v 1.1.2.2 2009/08/07 02:55:36 Exp $
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
# Install WLAN Driver, firmware, and configuration files.
#

ifeq ($(TARGET_PRODUCT),generic)
else
ifneq ($(TARGET_SIMULATOR),true) 

local_target_dir := $(TARGET_OUT)/wlan/broadcom

########################
LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_MODULE := fw_bcm4329.bin

LOCAL_MODULE_TAGS := user development

LOCAL_MODULE_CLASS := ETC
LOCAL_MODULE_PATH := $(local_target_dir)

LOCAL_SRC_FILES := $(LOCAL_MODULE)

include $(BUILD_PREBUILT)
########################
include $(CLEAR_VARS)

LOCAL_MODULE := fw_bcm4329_apsta.bin

LOCAL_MODULE_TAGS := user development

LOCAL_MODULE_CLASS := ETC
LOCAL_MODULE_PATH := $(local_target_dir)

LOCAL_SRC_FILES := $(LOCAL_MODULE)

include $(BUILD_PREBUILT)
########################
#remove by zhaoliang.mi nvram.txt to wcustores/wlan/
#include $(CLEAR_VARS)

#LOCAL_MODULE := nvram.txt

#LOCAL_MODULE_TAGS := user development

#LOCAL_MODULE_CLASS := ETC
#LOCAL_MODULE_PATH := $(local_target_dir)

#LOCAL_SRC_FILES := $(LOCAL_MODULE)

#include $(BUILD_PREBUILT)
#remove by zhaoliang.mi end

########################
include $(CLEAR_VARS)

LOCAL_MODULE := bcm4329.ko

LOCAL_MODULE_TAGS := user development

LOCAL_MODULE_CLASS := KERNEL_OBJ
LOCAL_MODULE_PATH := $(TARGET_OUT)/lib/modules

LOCAL_SRC_FILES := ../../../../../../$(TARGET_OUT_INTERMEDIATES)/KERNEL_OBJ/drivers/net/wireless/bcm4329/$(LOCAL_MODULE)

$(LOCAL_PATH)/$(LOCAL_SRC_FILES) : kernel

include $(BUILD_PREBUILT)

########################

include $(CLEAR_VARS)
LOCAL_MODULE := wpa_supplicant.conf
LOCAL_MODULE_TAGS := user development
LOCAL_MODULE_CLASS := ETC
LOCAL_MODULE_PATH := $(local_target_dir)
LOCAL_SRC_FILES := $(LOCAL_MODULE)
include $(BUILD_PREBUILT)

########################

include $(CLEAR_VARS)
LOCAL_MODULE := dhcpcd.conf
LOCAL_MODULE_TAGS := user development
LOCAL_MODULE_CLASS := ETC
LOCAL_MODULE_PATH := $(TARGET_OUT_ETC)/dhcpcd
LOCAL_SRC_FILES := android_dhcpcd.conf
include $(BUILD_PREBUILT)

########################
include $(CLEAR_VARS)

LOCAL_MODULE := wifi.conf

LOCAL_MODULE_TAGS := user development

LOCAL_MODULE_CLASS := ETC
LOCAL_MODULE_PATH := $(TARGET_OUT_ETC)

LOCAL_SRC_FILES := $(LOCAL_MODULE)

include $(BUILD_PREBUILT)

########################

endif  # TARGET_SIMULATOR != true
endif 
