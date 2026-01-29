#include <freertos/FreeRTOS.h>
#include <driver/uart.h>
#include <string.h>
#include "esp_event.h"
#include "esp_log.h"
#include "ibus/ibus.h"
#include "motor.h"
static const char* TAG = "MyModule";
static volatile ibus_channel_t last_channels[IBUS_CHANNEL_COUNT];
static volatile bool channels_updated = false;

static void channel_handler(ibus_channel_t *channels, void *cookie)
{
   memcpy((void*)last_channels, channels, sizeof(last_channels));
   channels_updated = true;
}
#define IBUS_UART   UART_NUM_1
#define IBUS_TX_PIN 4
#define IBUS_RX_PIN 5

void app_main(void) {
  ESP_LOGI(TAG, "app_main");
 ESP_ERROR_CHECK(esp_event_loop_create_default() );
  float puissance = 0.0;
   uart_lowlevel_config config;
   config.port = IBUS_UART;
   config.rx_pin = IBUS_RX_PIN;
     config.tx_pin = IBUS_TX_PIN;
   ibus_context_t *ctx = ibus_init(&config);
   if(NULL != ctx)
   {
      ibus_set_channel_handler(ctx, channel_handler, NULL);
   }
    pwminit();
    frontright(0.0);
   for(;;) {
      if (channels_updated) {
         channels_updated = false;
          puissance = (float)last_channels[2].value;
         ESP_LOGI(TAG, "CH: %f,", puissance);       }
      vTaskDelay(pdMS_TO_TICKS(100));
     frontright((puissance-1000.0)/10.0);

  }
   ibus_deinit(ctx);

} 
