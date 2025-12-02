set(srcs
        "main.c"
        "sketch.cpp"
        "bts7960.c"
        "ble_server.cpp")

set(requires "bluepad32" "bluepad32_arduino" "arduino" "btstack")

idf_component_register(SRCS "${srcs}"
                    INCLUDE_DIRS "."
                    REQUIRES "${requires}")
