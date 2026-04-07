#include <freertos/FreeRTOS.h>
#include <driver/uart.h>
#include "esp_event.h"
#include "esp_log.h"
#include "ibus/ibus.h"
#include "controller.h"

#define TAG         "ibus_debug"
#define IBUS_UART   UART_NUM_1
#define IBUS_TX_PIN 4
#define IBUS_RX_PIN 5

static void channel_handler(ibus_channel_t *channels, void *cookie)
{
    ESP_LOGI(TAG,
             "CH1-4: %04d %04d %04d %04d | CH5-8: %04d %04d %04d %04d",
             channels[0].value,  channels[1].value,  channels[2].value,  channels[3].value,
             channels[4].value,  channels[5].value,  channels[6].value,  channels[7].value);
}

void channelget(void)
{
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    uart_lowlevel_config config;
    config.port   = IBUS_UART;
    config.tx_pin = IBUS_TX_PIN;
    config.rx_pin = IBUS_RX_PIN;

    ibus_context_t *ctx = ibus_init(&config);
    if (ctx != NULL) {
        ibus_set_channel_handler(ctx, channel_handler, NULL);
    }

    for (;;)
        vTaskDelay(portMAX_DELAY);

    ibus_deinit(ctx);
}
