#include <stdio.h>
#include <cstring>
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "driver_RGBMatrix.h"
#include "RGBMatrix_device.h"
#include "font.h"
#include "timer.h"

#ifndef GPIO_START
#define GPIO_START 3
#endif
#ifndef GPIO_STOP
#define GPIO_STOP 4
#endif
#ifndef GPIO_RESET
#define GPIO_RESET 5
#endif

// Mask for GPIOs to use as Start, Stop and Reset buttons
#ifndef GPIO_MASK
#define GPIO_MASK (1 >> GPIO_START | 1 >> GPIO_STOP | 1 >> GPIO_RESET)
#endif

int8_t pageIdNumber = 0;
uint8_t selectSettingOptions = 1;
uint8_t set_hour_temp,set_min_temp,set_sec_temp;
uint16_t set_year_temp;
uint8_t set_mon_temp,set_mday_temp;
int8_t enterTimeSettingFlag = 1,selectedTimeOption = 0;
uint8_t BeepFlag=1,autoLightFlag = 0;
uint16_t autoLightCount = 0;
uint8_t AlarmFlag = 0;
uint16_t BeepCount = 0;
uint8_t systemStartupFlag = 1;
uint16_t adc_value;
extern uint8_t UpdateVideoMemory;


Timer timer = {Pause, 0, 0, 0, "000", "000"};
PDisplayDevice displayDevice;

void btn_callback(uint gpio, uint32_t events)
{
    switch (gpio)
    {
        case GPIO_START:
            timer.start();
            break;

        case GPIO_STOP:
            timer.stop();
            break;

        case GPIO_RESET:
            timer.reset();
            break;
    }
}

int main()
{
    stdio_init_all();
    
    /*gpio_init_mask(GPIO_MASK);
    gpio_set_dir_in_masked(GPIO_MASK);
    gpio_set_irq_enabled_with_callback(GPIO_START, GPIO_IRQ_EDGE_RISE, true, &btn_callback);
    // Only one callback function can be declared. Use btn_callback() for the other GPIOs as well.
    gpio_set_irq_enabled(GPIO_STOP, GPIO_IRQ_EDGE_RISE, true);
    gpio_set_irq_enabled(GPIO_RESET, GPIO_IRQ_EDGE_RISE, true);
    gpio_set_pulls(GPIO_START, true, false);
    gpio_set_pulls(GPIO_STOP, true, false);
    gpio_set_pulls(GPIO_RESET, true, false);
    */

    displayDevice = GetDisplayDevice();
    displayDevice->Init();
    

    while(true)
    {
        timer.run();

        memset(displayDevice->FBBase,0,256);
        displayDevice->SetPixel(displayDevice, 5, 22, 'A', DAT8X10);
        select_area_color(1, 6, 43, 15, Purple, LOW_ROW);
        //displayDevice->SetPixel(displayDevice, 15, 10, 'A', DAT4X7);

        // TODO temp code: slow sleep for serial comm
        sleep_ms(10);
        printf("%s:%s\n", timer.sec_counter, timer.ms_counter);
    }

    return 0;
}
