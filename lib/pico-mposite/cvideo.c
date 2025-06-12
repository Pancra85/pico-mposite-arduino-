#include <Arduino.h>
#include <stdlib.h>

#include "memory.h"
#include "pico/stdlib.h"

#include "hardware/pio.h"
#include "hardware/dma.h"
#include "hardware/irq.h"

#include "charset.h" // The character set
#include "cvideo.h"
#include "graphics.h"
#include "cvideo_sync.pio.h" // The assembled PIO code
#include "cvideo_data.pio.h"

// --- PAL/NTSC: Defines de líneas y rangos de sincronía/border ---
#if VIDEO_NTSC
#define NTSC_TOTAL_LINES 524
#define NTSC_FIELD_LINES 249 // era 269. 321 para 312pixeles de alto. 309 para 240 pero raro
#define NTSC_VSYNC_START 1
#define NTSC_VSYNC_END 6
#define NTSC_VSYNC_SHORT_START 7
#define NTSC_VSYNC_SHORT_END 9
#define NTSC_BORDER_TOP_START 0
#define NTSC_BORDER_TOP_END 0        // antes 38
#define NTSC_BORDER_BOTTOM_START 270 // antes 223
#define NTSC_BORDER_BOTTOM_END 262
#else
#define VIDEO_TOTAL_LINES 258
#define VSYNC_LL1_START 1
#define VSYNC_LL1_END 2
#define VSYNC_LS_LINE 3
#define VSYNC_SS_START 4
#define VSYNC_SS_END 5
#define BORDER_TOP_START 6
#define BORDER_TOP_END 18
#define BORDER_BOTTOM_START 307
#define BORDER_BOTTOM_END 309
#endif

#define HSYNC_TABLE_SIZE 32

int screenWidth = 320;
int screenHeight = 240;

#if VIDEO_NTSC
int ntsc_field = 0; // 0: even, 1: odd
#endif

#if VIDEO_NTSC
unsigned short hsync[HSYNC_TABLE_SIZE] = {
    HSLO, HSLO, HSHI, HSHI, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, BORD};
#else //para PAL:
unsigned short hsync[HSYNC_TABLE_SIZE] = {
    HSLO, HSLO, HSHI, HSHI, HSHI, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, BORD};
    #endif

unsigned short border[HSYNC_TABLE_SIZE] = {
    HSLO, HSLO, HSHI, HSHI, HSHI, HSHI, BORD, // sync pulses + 1 borde izq
    BORD, BORD, BORD, BORD, BORD, BORD, BORD,
    BORD, BORD, BORD, BORD, BORD, BORD, BORD,
    BORD, BORD, BORD, BORD, BORD, BORD, BORD,
    BORD, BORD, BORD, BORD // área de borde
};

unsigned short vsync_ll[HSYNC_TABLE_SIZE] = {
    VSLO, VSLO, VSLO, VSLO, VSLO, VSLO, VSLO, VSLO, VSLO, VSLO, VSLO, VSLO, VSLO, VSLO, VSLO, VSHI, // 16
    VSLO, VSLO, VSLO, VSLO, VSLO, VSLO, VSLO, VSLO, VSLO, VSLO, VSLO, VSLO, VSLO, VSLO, VSLO, VSHI  // 32
};

unsigned short vsync_ss[HSYNC_TABLE_SIZE] = {
    VSLO, VSHI, VSHI, VSHI, VSHI, VSHI, VSHI, VSHI, VSHI, VSHI, VSHI, VSHI, VSHI, VSHI, VSHI, VSHI, // 16
    VSLO, VSHI, VSHI, VSHI, VSHI, VSHI, VSHI, VSHI, VSHI, VSHI, VSHI, VSHI, VSHI, VSHI, VSHI, VSHI  // 32
};

unsigned short vsync_ls[HSYNC_TABLE_SIZE] = {
    VSLO, VSLO, VSLO, VSLO, VSLO, VSLO, VSLO, VSLO, VSLO, VSLO, VSLO, VSLO, VSLO, VSLO, VSLO, VSHI, // 16
    VSLO, VSHI, VSHI, VSHI, VSHI, VSHI, VSHI, VSHI, VSHI, VSHI, VSHI, VSHI, VSHI, VSHI, VSHI, VSHI  // 32
};

PIO pio_0;     // The PIO that this uses
uint offset_0; // Program offsets
uint offset_1;

uint dma_channel_0; // DMA channel for transferring sync data to PIO
uint dma_channel_1; // DMA channel for transferring pixel data data to PIO
uint vline;         // Current PAL(ish) video line being processed
uint bline;         // Line in the bitmap to fetch

uint vblank_count; // Vblank counter

unsigned char *screen_bitmap = NULL;
unsigned char *screen_bitmap_next = NULL;
unsigned char *screen_bitmap_a = NULL;
unsigned char *screen_bitmap_b = NULL;

void swap_video_buffer()
{
    unsigned char *tmp = screen_bitmap;
    screen_bitmap = screen_bitmap_next;
    screen_bitmap_next = tmp;
}

int initialise_cvideo(void)
{
    pio_0 = pio0; // Assign the PIO

    // Load up the PIO programs
    //
    offset_0 = pio_add_program(pio_0, &cvideo_sync_program);
    offset_1 = pio_add_program(pio_0, &cvideo_data_program);

    dma_channel_0 = dma_claim_unused_channel(true); // Claim a DMA channel for the sync
    dma_channel_1 = dma_claim_unused_channel(true); // And one for the pixel data

    vline = 1;        // Initialise the video scan line counter to 1
    bline = 0;        // And the index into the bitmap pixel buffer to 0
    vblank_count = 0; // And the vblank counter

    // Initialise the first PIO (video sync)
    //
    pio_sm_set_enabled(pio_0, sm_sync, false); // Disable the PIO state machine
    pio_sm_clear_fifos(pio_0, sm_sync);        // Clear the PIO FIFO buffers
    cvideo_sync_initialise_pio(                // Initialise the PIO (function in cvideo.pio)
        pio_0,                                 // The PIO to attach this state machine to
        sm_sync,                               // The state machine number
        offset_0,                              // And offset
        gpio_base,                             // Start pin in the GPIO
        gpio_count,                            // Number of pins
        piofreq_0                              // State machine clock frequency
    );
    cvideo_configure_pio_dma( // Configure the DMA
        pio_0,                // The PIO to attach this DMA to
        sm_sync,              // The state machine number
        dma_channel_0,        // The DMA channel
        DMA_SIZE_16,          // Size of each transfer
        HSYNC_TABLE_SIZE,     // <--- Cambiado de 32 a HSYNC_TABLE_SIZE
        cvideo_dma_handler    // The DMA handler
    );

    // Allocate double buffers
    size_t bufsize = screenWidth * screenHeight;
    screen_bitmap_a = malloc(bufsize);
    screen_bitmap_b = malloc(bufsize);
    screen_bitmap = screen_bitmap_a;
    screen_bitmap_next = screen_bitmap_b;

    // Initialise the second PIO (pixel data)
    //
    cvideo_data_initialise_pio(
        pio_0,
        sm_data,
        offset_1,
        gpio_base,
        gpio_count,
        piofreq_1_256);

    // Initialise the DMA
    //
    cvideo_configure_pio_dma(
        pio_0,
        sm_data,
        dma_channel_1, // On DMA channel 1
        DMA_SIZE_8,    // Size of each transfer
        screenWidth,   // The bitmap screenWidth
        NULL           // But there is no DMA interrupt for the pixel data
    );

    irq_set_exclusive_handler( // Set up the PIO IRQ handler
        PIO0_IRQ_0,            // The IRQ #
        cvideo_pio_handler     // And handler routine
    );
    pio0_hw->inte0 = PIO_IRQ0_INTE_SM0_BITS; // Just for IRQ 0 (triggered by irq set 0 in PIO)
    irq_set_enabled(PIO0_IRQ_0, true);       // Enable it

    set_border(0);                          // Set the border colour
    clearScreen(0);                         // Clear the screen (front buffer)
    memset(screen_bitmap_next, 0, bufsize); // Clear the back buffer

    // Start the PIO state machines
    //
    pio_enable_sm_mask_in_sync(pio_0, (1u << sm_data) | (1u << sm_sync));
    return 0;
}

// Set the graphics mode
// mode - The graphics mode (0 = 256x192, 1 = 320 x 192, 2 = 640 x 192)
//
int set_mode(int mode)
{
    double dfreq;

    wait_vblank();

    switch (mode)
    { // Get the video mode
    case 1:
        screenWidth = 320;     // Set screen screenWidth and
        dfreq = piofreq_1_320; // pixel dot frequency accordingly
        break;
    case 2:
        screenWidth = 640;
        dfreq = piofreq_1_640;
        break;
    default:
        screenWidth = 256;
        dfreq = piofreq_1_256;
        break;
    }
    // Free and reallocate both buffers if mode changes
    if (screen_bitmap_a)
        free(screen_bitmap_a);
    if (screen_bitmap_b)
        free(screen_bitmap_b);
    size_t bufsize = screenWidth * screenHeight;
    screen_bitmap_a = malloc(bufsize);
    screen_bitmap_b = malloc(bufsize);
    screen_bitmap = screen_bitmap_a;
    screen_bitmap_next = screen_bitmap_b;
    clearScreen(0);
    memset(screen_bitmap_next, 0, bufsize);

    cvideo_configure_pio_dma( // Reconfigure the DMA
        pio_0,
        sm_data,
        dma_channel_1, // On DMA channel 1
        DMA_SIZE_8,    // Size of each transfer
        screenWidth,   // The bitmap screenWidth
        NULL           // But there is no DMA interrupt for the pixel data
    );

    pio_0->sm[sm_data].clkdiv = (uint32_t)(dfreq * (1 << 16));

    return 0;
}

// Set the border colour
// - colour: Border colour
//
void set_border(unsigned char colour)
{
    if (colour > colour_max)
    {
        return;
    }
    unsigned short c = BORD | (colour_base + colour);

    for (int i = 6; i < HSYNC_TABLE_SIZE; i++)
    { // Skip the first tres hsync values
        if (hsync[i] & BORD)
        {                 // If the border bit is set in the hsync
            hsync[i] = c; // Then write out the new colour (with the BORD bit set)
        }
        border[i] = c; // We can just write the values out to the border table
    }
}

// Wait for vblank
//
void wait_vblank(void)
{
    uint c = vblank_count; // Get the current vblank count
    while (c == vblank_count)
    {                // Wait until it changes
        sleep_us(4); // Need to sleep for a minimum of 4us
    }
}

// The PIO interrupt handler
// This sets up the DMA for cvideo_data with pixel data and is triggered by the irq set 0
// instruction at the end of the PIO
//
void cvideo_pio_handler(void)
{
    if (bline >= screenHeight)
    {
        bline = 0;
    }
    dma_channel_set_read_addr(dma_channel_1, &screen_bitmap[screenWidth * bline++], true); // Line up the next block of pixels
    hw_set_bits(&pio0->irq, 1u);                                                           // Reset the IRQ
}

// The DMA interrupt handler
// This feeds the state machine cvideo_sync with data for the PAL(ish) video signal
//
void cvideo_dma_handler(void)
{
#if VIDEO_NTSC
    // NTSC "no entrelazado": 262 líneas por campo, sin media línea
    int field_line = vline;
    // VSYNC: 6 líneas de long sync, 3 de short sync (puedes ajustar si tu capturadora es muy exigente)
    if (field_line >= NTSC_VSYNC_START && field_line <= NTSC_VSYNC_END)
    {
        dma_channel_set_read_addr(dma_channel_0, vsync_ll, true);
    }
    else if (field_line >= NTSC_VSYNC_SHORT_START && field_line <= NTSC_VSYNC_SHORT_END)
    {
        dma_channel_set_read_addr(dma_channel_0, vsync_ss, true);
    }
    else if (field_line >= NTSC_BORDER_TOP_START && field_line <= NTSC_BORDER_TOP_END)
    {
        dma_channel_set_read_addr(dma_channel_0, border, true);
    }
    else if (field_line >= NTSC_BORDER_BOTTOM_START && field_line <= NTSC_BORDER_BOTTOM_END)
    {
        dma_channel_set_read_addr(dma_channel_0, border, true);
    }
    else
    {
        dma_channel_set_read_addr(dma_channel_0, hsync, true);
    }
    // Flip de campo y reset cada 262 líneas
    if (vline++ >= NTSC_FIELD_LINES)
    {
        vline = 1;
        ntsc_field ^= 1;
        vblank_count++;
    }
#else
    switch (vline)
    {
    case VSYNC_LL1_START ... VSYNC_LL1_END:
        dma_channel_set_read_addr(dma_channel_0, vsync_ll, true);
        break;
    case VSYNC_LS_LINE:
        dma_channel_set_read_addr(dma_channel_0, vsync_ls, true);
        break;
    case VSYNC_SS_START ... VSYNC_SS_END:
    case 310:
    case 311:
    case 312:
        dma_channel_set_read_addr(dma_channel_0, vsync_ss, true);
        break;
    case BORDER_TOP_START ... BORDER_TOP_END:
        dma_channel_set_read_addr(dma_channel_0, border, true);
        break;
    case BORDER_BOTTOM_START ... 309:
        dma_channel_set_read_addr(dma_channel_0, border, true);
        break;
    default:
        dma_channel_set_read_addr(dma_channel_0, hsync, true);
        break;
    }
    if (vline++ >= VIDEO_TOTAL_LINES)
    {
        vline = 1;
        vblank_count++;
    }
#endif
    dma_hw->ints0 = 1u << dma_channel_0;
}

// Configure the PIO DMA
// Parameters:
// - pio: The PIO to attach this to
// - sm: The state machine number
// - dma_channel: The DMA channel
// - transfer_size: Size of each DMA bus transfer (DMA_SIZE_8, DMA_SIZE_16 or DMA_SIZE_32)
// - buffer_size_words: Number of bytes to transfer
// - handler: Address of the interrupt handler, or NULL for no interrupts
//
void cvideo_configure_pio_dma(PIO pio, uint sm, uint dma_channel, uint transfer_size, size_t buffer_size, irq_handler_t handler)
{
    dma_channel_config c = dma_channel_get_default_config(dma_channel);
    channel_config_set_transfer_data_size(&c, transfer_size);
    channel_config_set_read_increment(&c, true);
    channel_config_set_dreq(&c, pio_get_dreq(pio, sm, true));
    dma_channel_configure(dma_channel, &c,
                          &pio->txf[sm], // Destination pointer
                          NULL,          // Source pointer
                          buffer_size,   // Size of buffer
                          true           // Start flag (true = start immediately)
    );
    if (handler != NULL)
    {
        dma_channel_set_irq0_enabled(dma_channel, true);
        irq_set_exclusive_handler(DMA_IRQ_0, handler);
        irq_set_enabled(DMA_IRQ_0, true);
    }
}
