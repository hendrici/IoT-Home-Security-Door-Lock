idf_component_register(SRCS "lock_main.c" "lcd.c" "lock.c"
                       INCLUDE_DIRS ".")

# Set the target board type (replace "DEFAULT_TARGET_ESP32" with your specific board variant)
target_compile_definitions(${COMPONENT_LIB} PRIVATE "-DEFAULT_TARGET_ESP32")

target_compile_features(${COMPONENT_LIB} PRIVATE c_std_11)
