cmake_minimum_required(VERSION 3.16)

if(NOT DEFINED DEVICE_ID)
    set(DEVICE_ID "esp_device_1") # Default
endif()

if(NOT DEFINED BROKER_URL)
    set(BROKER_URL "mqtt://broker.hivemq.com")  # Default
endif()

if(NOT DEFINED TOPIC)
    set(TOPIC "esp-device-default/data")
endif()

add_compile_definitions(
    DEVICE_ID="${DEVICE_ID}"
    BROKER_URL="${BROKER_URL}"
    TOPIC="${TOPIC}"
)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(mqtt_tcp)

