#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "hardware/dma.h"
#include "hardware/irq.h"
#include "pico/time.h"
#include "capture.pio.h"


#define PULSE_THRESHOLD 6
#define SKIP 128

absolute_time_t pulse_start_time,pulse_end_time;

uint32_t cap_line_setup=(SKIP) | (511<<16);
//volatile uint32_t tst=0;
//uint32_t *ptst=&tst;


int data_dma;
volatile uint8_t cap_buf[64*1024];
uint16_t cap_line=0;


void __time_critical_func(pulse_begin)(void)
{
	pulse_start_time=get_absolute_time();
	pio_interrupt_clear(pio1,2);
}


void __time_critical_func(pulse_end)(void)
{
	pulse_end_time=get_absolute_time();
	if(absolute_time_diff_us(pulse_start_time,pulse_end_time) > PULSE_THRESHOLD)
	{
		// VS detected
		cap_line=0;
	}
	pio_interrupt_clear(pio1,3);
}

void __time_critical_func(cap_dma_handler)(void)
{
	dma_hw->ints1 = 1u << data_dma;
	cap_line++;
	if(cap_line>1000) cap_line=0;
	dma_channel_set_write_addr(data_dma,cap_buf+(cap_line*64),true);

}




void capture_dma_init(void)
{
	dma_channel_config dma_conf;
	int dma1,dma2;

	dma1=dma_claim_unused_channel(true);
	dma2=dma_claim_unused_channel(true);
	dma_conf=dma_channel_get_default_config(dma1);
	channel_config_set_transfer_data_size(&dma_conf, DMA_SIZE_32);
	channel_config_set_read_increment(&dma_conf, false);
	channel_config_set_write_increment(&dma_conf, false);
	channel_config_set_chain_to(&dma_conf,dma2);
	channel_config_set_dreq(&dma_conf, DREQ_PIO1_TX0);
	dma_channel_configure(
		dma1,                                     // Channel to be configured
		&dma_conf,                                    // The configuration we just created
        &pio1_hw->txf[0],							  // The initial write address PIO0_TX0
        &cap_line_setup,                                    // The initial read address
        256,                 							  // Number of transfers.
        false                                  		  // Not start immediately.
		);

	dma_conf=dma_channel_get_default_config(dma2);
	channel_config_set_transfer_data_size(&dma_conf, DMA_SIZE_32);
	channel_config_set_read_increment(&dma_conf, false);
	channel_config_set_write_increment(&dma_conf, false);
	channel_config_set_chain_to(&dma_conf,dma1);
	channel_config_set_dreq(&dma_conf, DREQ_PIO1_TX0);
	dma_channel_configure(
		dma2,                                     // Channel to be configured
		&dma_conf,                                    // The configuration we just created
        &pio1_hw->txf[0],							  // The initial write address PIO0_TX0
        &cap_line_setup,                                    // The initial read address
        256,                 							  // Number of transfers.
        false                                  		  // Not start immediately.
		);


	dma_channel_start(dma1);

	data_dma=dma_claim_unused_channel(true);
	dma_conf=dma_channel_get_default_config(data_dma);
	channel_config_set_transfer_data_size(&dma_conf, DMA_SIZE_32);
	channel_config_set_read_increment(&dma_conf, false);
	channel_config_set_write_increment(&dma_conf, true);
	channel_config_set_chain_to(&dma_conf,data_dma);
	channel_config_set_dreq(&dma_conf, DREQ_PIO1_RX0);
	dma_channel_configure(
		data_dma,                                     // Channel to be configured
		&dma_conf,                                    // The configuration we just created
		cap_buf,						  // The initial write address PIO0_RX0
        &pio1_hw->rxf[0],                                    // The initial read address
        16,                 							  // Number of transfers.
        true                                  		  // start immediately.
		);
	dma_channel_set_irq1_enabled(data_dma, true);
	irq_set_exclusive_handler(DMA_IRQ_1, cap_dma_handler);
	irq_set_enabled(DMA_IRQ_1, true);

}

void capture_init(void)
{
	uint offset0;
	PIO pio = pio1;
    uint sm0 = 0;

	for(int i=0;i<64*512;i++) cap_buf[i]=(i>>2)&0xff;

	pio_set_irq0_source_enabled(pio, pis_interrupt2, true);
	pio_set_irq1_source_enabled(pio, pis_interrupt3, true);
	irq_set_exclusive_handler(PIO1_IRQ_0,pulse_begin);
	irq_set_exclusive_handler(PIO1_IRQ_1,pulse_end);
	irq_set_enabled(PIO1_IRQ_0, true);
	irq_set_enabled(PIO1_IRQ_1, true);

	offset0 = pio_add_program(pio, &capture_program);
	capture_program_init(pio, sm0, offset0,6); // pins 6,7,8

	capture_dma_init();

}
