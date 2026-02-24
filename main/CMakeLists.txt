idf_component_register(SRCS "main.c"
                            "motor.c"
                            "stabilization.c"
                            "controller.c"
                            "pid.c"
                            "flight_controller.c"
                    INCLUDE_DIRS "."
                    REQUIRES driver
                    PRIV_REQUIRES esp_event
                    )
