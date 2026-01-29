idf_component_register(SRCS "main.c" "motor.c"
                    INCLUDE_DIRS "."
                  REQUIRES driver
                  PRIV_REQUIRES esp_event
                  )
