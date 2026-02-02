# Spanish Braille component for ESP-IDF (Grade 1, scalable cell drawing)
idf_component_register(
    SRCS "src/spanish_braille.c"
    INCLUDE_DIRS "include"
    REQUIRES u8g2
)
