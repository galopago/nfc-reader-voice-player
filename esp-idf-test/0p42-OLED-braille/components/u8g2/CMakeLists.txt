# U8g2 library for ESP-IDF (olikraus/u8g2)
# All csrc/*.c are included; full buffer + fonts.
file(GLOB_RECURSE SOURCES "csrc/*.c")
idf_component_register(SRCS ${SOURCES}
                       INCLUDE_DIRS "csrc")
