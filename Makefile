#
# This is a project Makefile. It is assumed the directory this Makefile resides in is a
# project subdirectory.
#

CONFIG_TOOLPREFIX ?= "../../Tools_Downloads/xtensa-esp32-elf/bin/xtensa-esp32-elf-"
IDF_PATH ?= ../../esp/esp-idf

PROJECT_NAME := my_pwm
SRCDIRS := .
CONFIG_WIFI_SSID = "yyy"
CONFIG_WIFI_PASS = "xxx"

include $(IDF_PATH)/make/project.mk
