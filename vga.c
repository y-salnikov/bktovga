

#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "hardware/dma.h"
#include "hardware/irq.h"
#include "vga.pio.h"
/*
Horizontal timing (line)
Polarity of horizontal sync pulse is negative.

Scanline part	Pixels	Time [µs]
Visible area	1024	15.753846153846
Front porch		24		0.36923076923077
Sync pulse		136		2.0923076923077
Back porch		160		2.4615384615385
Whole line		1344	20.676923076923

Vertical timing (frame)
Polarity of vertical sync pulse is negative.

Frame part		Lines	Time [ms]
Visible area	768		15.879876923077
Front porch		3		0.062030769230769
Sync pulse		6		0.12406153846154
Back porch		29		0.59963076923077
Whole frame		806		16.6656
*/

#define HS_BIT 1
#define VS_BIT 2
#define DE_BIT 4


#define V_OFFSET 41


extern uint8_t cap_buf[64*512];

uint8_t mode=0;

uint32_t sync_line[]=         {HS_BIT|VS_BIT|DE_BIT|(1022<<3),   HS_BIT|VS_BIT|(22<<3),   VS_BIT|(134<<3),      HS_BIT|VS_BIT|(158<<3)};       // 1024x768@60
uint32_t sync_line_vs_porch[]={HS_BIT|VS_BIT|       (1022<<3),   HS_BIT|VS_BIT|(22<<3),   VS_BIT|(134<<3),      HS_BIT|VS_BIT|(158<<3)};
uint32_t sync_line_vs_pulse[]={HS_BIT|              (1022<<3),   HS_BIT|       (22<<3),          (134<<3),      HS_BIT|       (158<<3)};

uint32_t dma_table[806];
int line_restart_dma;
uint32_t current_line=0;
uint32_t pxbuf1[128];
uint32_t pxbuf2[128];
int line_dma,px_dma[6];

const uint32_t ra_trigger[]={DMA_CH0_AL3_READ_ADDR_TRIG_OFFSET,
							 DMA_CH1_AL3_READ_ADDR_TRIG_OFFSET,
							 DMA_CH2_AL3_READ_ADDR_TRIG_OFFSET,
							 DMA_CH3_AL3_READ_ADDR_TRIG_OFFSET,
							 DMA_CH4_AL3_READ_ADDR_TRIG_OFFSET,
							 DMA_CH5_AL3_READ_ADDR_TRIG_OFFSET,
							 DMA_CH6_AL3_READ_ADDR_TRIG_OFFSET,
							 DMA_CH7_AL3_READ_ADDR_TRIG_OFFSET,
							 DMA_CH8_AL3_READ_ADDR_TRIG_OFFSET,
							 DMA_CH9_AL3_READ_ADDR_TRIG_OFFSET,
							 DMA_CH10_AL3_READ_ADDR_TRIG_OFFSET,
							 DMA_CH11_AL3_READ_ADDR_TRIG_OFFSET,
							 };

uint32_t col_tab_bw[16]={ 0x00000000,  //0000
						  0x00000077,  //0001
						  0x00007700,  //0010
						  0x00007777,  //0011
						  0x00770000,  //0100
						  0x00770077,  //0101
						  0x00777700,  //0110
						  0x00777777,  //0111
						  0x77000000,  //1000
						  0x77000077,  //1001
						  0x77007700,  //1010
						  0x77007777,  //1011
						  0x77770000,  //1100
						  0x77770077,  //1101
						  0x77777700,  //1110
						  0x77777777  //1111
						  };

uint32_t col_tab_c[16]={ 0x00000000,  //0000
						  0x00004444,  //0001
						  0x00002222,  //0010
						  0x00001111,  //0011
						  0x44440000,  //0100
						  0x44444444,  //0101
						  0x44442222,  //0110
						  0x44441111,  //0111
						  0x22220000,  //1000
						  0x22224444,  //1001
						  0x22222222,  //1010
						  0x22221111,  //1011
						  0x11110000,  //1100
						  0x11114444,  //1101
						  0x11112222,  //1110
						  0x11111111  //1111
						  };



void fill_line(uint32_t line, uint32_t *buf);


void __time_critical_func(dma_handler)(void)
{
	dma_hw->ints0 = 1u << line_restart_dma;
	if(current_line==769)
	{
//			dma_channel_wait_for_finish_blocking(px_dma[2]);
			dma_channel_set_read_addr(px_dma[0],pxbuf1,false);
			dma_channel_set_read_addr(px_dma[1],pxbuf1,false);
			dma_channel_set_read_addr(px_dma[2],pxbuf1,false);
			fill_line(0,pxbuf1);
//			dma_channel_wait_for_finish_blocking(px_dma[5]);
			dma_channel_set_read_addr(px_dma[3],pxbuf2,false);
			dma_channel_set_read_addr(px_dma[4],pxbuf2,false);
			dma_channel_set_read_addr(px_dma[5],pxbuf2,false);
			fill_line(1,pxbuf2);

	} else
	if(current_line<768)
	{
		if((current_line%3)==2)
		{
			if((current_line/3)%2==0)
			{
				dma_channel_wait_for_finish_blocking(px_dma[2]);
				dma_channel_set_read_addr(px_dma[0],pxbuf1,false);
				dma_channel_set_read_addr(px_dma[1],pxbuf1,false);
				dma_channel_set_read_addr(px_dma[2],pxbuf1,false);
				fill_line((current_line/3)+2,pxbuf1);
			}
			else
			{
				dma_channel_wait_for_finish_blocking(px_dma[5]);
				dma_channel_set_read_addr(px_dma[3],pxbuf2,false);
				dma_channel_set_read_addr(px_dma[4],pxbuf2,false);
				dma_channel_set_read_addr(px_dma[5],pxbuf2,false);
				fill_line((current_line/3)+2,pxbuf2);
			}
		}
	}
	current_line++;
	if(current_line==806)
	{
		current_line=0;
		dma_channel_set_read_addr(line_restart_dma, dma_table, false);
	}
}

void vga_dma_table_fill(void)
{
	int i;
	for(i=0;i<768;i++)
	{
		dma_table[i]=(uint32_t)sync_line;
	}
	dma_table[i++]=(uint32_t)sync_line_vs_porch;
	dma_table[i++]=(uint32_t)sync_line_vs_porch;
	dma_table[i++]=(uint32_t)sync_line_vs_porch;
	dma_table[i++]=(uint32_t)sync_line_vs_pulse;
	dma_table[i++]=(uint32_t)sync_line_vs_pulse;
	dma_table[i++]=(uint32_t)sync_line_vs_pulse;
	dma_table[i++]=(uint32_t)sync_line_vs_pulse;
	dma_table[i++]=(uint32_t)sync_line_vs_pulse;
	dma_table[i++]=(uint32_t)sync_line_vs_pulse;
	for(;i<806;i++)
	{
		dma_table[i]=(uint32_t)sync_line_vs_porch;
	}
}

void vga_dma_init(void)
{
	int i;
	dma_channel_config dma_conf,dma_r_conf;

	dma_channel_config px_dma_cfg[6];

	line_dma=dma_claim_unused_channel(true);
	line_restart_dma=dma_claim_unused_channel(true);
	dma_conf=dma_channel_get_default_config(line_dma);
	channel_config_set_transfer_data_size(&dma_conf, DMA_SIZE_32);
	channel_config_set_read_increment(&dma_conf, true);
	channel_config_set_write_increment(&dma_conf, false);
	channel_config_set_chain_to(&dma_conf,line_restart_dma);
	channel_config_set_dreq(&dma_conf, DREQ_PIO0_TX1);
	dma_channel_configure(
		line_dma,                                     // Channel to be configured
		&dma_conf,                                    // The configuration we just created
        &pio0_hw->txf[1],							  // The initial write address PIO0_TX1
        sync_line,                                    // The initial read address
        4,                 							  // Number of transfers.
        false                                  		  // Not start immediately.
		);

	dma_r_conf=dma_channel_get_default_config(line_restart_dma);
	channel_config_set_transfer_data_size(&dma_r_conf, DMA_SIZE_32);
	channel_config_set_read_increment(&dma_r_conf, true);
	channel_config_set_write_increment(&dma_r_conf, false);
	channel_config_set_chain_to(&dma_conf,line_restart_dma);
	dma_channel_configure(
		line_restart_dma,                             // Channel to be configured
		&dma_r_conf,                                  // The configuration we just created
        (uint32_t *)(DMA_BASE+ra_trigger[line_dma]),  // The initial write address
        dma_table,                                    // The initial read address
        1,                 							  // Number of transfers.
        false                                  		  // Not start immediately.
		);
	dma_channel_set_irq0_enabled(line_restart_dma, true);
	irq_set_exclusive_handler(DMA_IRQ_0, dma_handler);
    irq_set_enabled(DMA_IRQ_0, true);


	for(i=0;i<6;i++)
	{
		px_dma[i]=dma_claim_unused_channel(true);
		px_dma_cfg[i]=dma_channel_get_default_config(px_dma[i]);
		channel_config_set_transfer_data_size(&px_dma_cfg[i], DMA_SIZE_32);
		channel_config_set_read_increment(&px_dma_cfg[i], true);
		channel_config_set_write_increment(&px_dma_cfg[i], false);
		channel_config_set_dreq(&px_dma_cfg[i], DREQ_PIO0_TX2);
	}
	channel_config_set_chain_to(&px_dma_cfg[0],px_dma[1]);
	channel_config_set_chain_to(&px_dma_cfg[1],px_dma[2]);
	channel_config_set_chain_to(&px_dma_cfg[2],px_dma[3]);
	channel_config_set_chain_to(&px_dma_cfg[3],px_dma[4]);
	channel_config_set_chain_to(&px_dma_cfg[4],px_dma[5]);
	channel_config_set_chain_to(&px_dma_cfg[5],px_dma[0]);
	for(i=0;i<6;i++)
	{
		dma_channel_configure(
			px_dma[i],                                     // Channel to be configured
			&px_dma_cfg[i],                                // The configuration we just created
			&pio0_hw->txf[2],							  // The initial write address PIO0_TX2
			(i<3)?pxbuf1:pxbuf2,                                       // The initial read address
			128,                 	    				  // Number of transfers.
			false                                  		  // Not start immediately.
			);

	}


}

void vga_dma_start(void)
{
	dma_channel_start(px_dma[0]);
	dma_channel_start(line_dma);
}

void __time_critical_func(fill_line)(uint32_t line, uint32_t *buf)
{
	uint8_t c,c1,c2;
	uint32_t *col_tab;

	if(mode==0)
	{
		col_tab=col_tab_bw;
	}
	else
	{
		col_tab=col_tab_c;
	}

	for(int i=0; i<64;i++)
	{
		c=~cap_buf[((line+V_OFFSET)<<6)+i];
		c1=c&0x0f; 	c2=c>>4;

		buf[i<<1]=col_tab[c1];
		buf[(i<<1)+1]=col_tab[c2];
	}

}

void vga_init(void)
{
	uint offset0,offset1,offset2;
	PIO pio = pio0;
    uint sm0 = 0;
    uint sm1 = 1;
    uint sm2 = 2;

	vga_dma_table_fill();
	vga_dma_init();

	uint32_t cols[8]={0x00000000,0x11111111,0x22222222,0x33333333,0x44444444,0x55555555,0x66666666,0x77777777};

	for(int i=0; i<128;i++)
	{

		pxbuf1[i]=cols[i/16];// & 0xF0F0F0F0;
		pxbuf2[i]=cols[i/16];// & 0x0F0F0F0F;
	}

	offset0 = pio_add_program(pio, &vga_clk_program);
	offset1 = pio_add_program(pio, &vga_sync_program);
	offset2 = pio_add_program(pio, &vga_px_program);

	vga_clk_program_init(pio, sm0, offset0);
	vga_px_program_init(pio, sm2, offset2,0); // pins 0,1,2
	vga_sync_program_init(pio, sm1, offset1,3); // pins 3,4,5
	vga_dma_start();


}




