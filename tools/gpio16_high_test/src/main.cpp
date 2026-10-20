#include <driver/gpio.h>

// Use the IDF entry directly: do not start Arduino or the Normal One application.
extern "C" void app_main() {
  gpio_set_level(GPIO_NUM_16, 1);
  gpio_set_direction(GPIO_NUM_16, GPIO_MODE_OUTPUT);
  // Returning deletes the main task; GPIO16 stays HIGH while the RTOS idles.
}
