idf_component_register(
    SRCS
        "main.c"
        "cnc_firmware_rtos.c"
        "accel_task.c"
        "pressure_task.c"
        "ultrasonic_task.c"
        "housekeeping_task.c"
        "barcode_task.c"
        "rfid_task.c"
        "aggregator_task.c"
        "analytics_task.c"
        "wifi_server.c"
        "mqtt_task.c"
    INCLUDE_DIRS "."
)