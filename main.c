#include "pico/stdlib.h"
#include "hardware/clocks.h"
#include "vga.h"
#include "capture.h"
#include "pico/multicore.h"

#define  LED_PIN 25
#define BUTTON_PIN 18
#define BUTTON_PIN_GND 19

extern uint8_t mode;

void core1_main(void)
{

    uint8_t state=0;
    uint8_t *pmode=&mode;
	capture_init();
	gpio_init(BUTTON_PIN);
	gpio_init(BUTTON_PIN_GND);
	gpio_set_dir(BUTTON_PIN, GPIO_IN);
	gpio_set_dir(BUTTON_PIN_GND, GPIO_OUT);
	gpio_put(BUTTON_PIN_GND,0);
	gpio_pull_up(BUTTON_PIN);
	while(1)
	{
		if (state==0)
		{
			if(gpio_get(BUTTON_PIN)==0)
			{
				state=1;
				*pmode=1-*pmode;
				sleep_ms(100);
			}
		}else
		{
			if(gpio_get(BUTTON_PIN)!=0)
			{
				state=0;
			}
		}

	}
}



int main()
{
	set_sys_clock_khz(260000,true);
    gpio_init(LED_PIN);

    gpio_set_dir(LED_PIN, GPIO_OUT);

	vga_init();
	multicore_launch_core1(core1_main);


    while (1)
    {
        gpio_put(LED_PIN, 0);
        sleep_ms(25);
        gpio_put(LED_PIN, 1);
        sleep_ms(25);
    }
}
