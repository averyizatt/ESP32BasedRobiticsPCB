// =============================================================================
// TFT_eSPI User Setup — ST7735S 0.96" 160×80 on ESP32-S3
// Pins match config.h:  MOSI=37  SCLK=38  CS=35  DC=36  RST=39
// =============================================================================

#define ST7735_DRIVER

// Native (portrait) framebuffer dimensions — library swaps on rotation
#define TFT_WIDTH   80
#define TFT_HEIGHT  160

// Pin assignments
#define TFT_MOSI    37
#define TFT_SCLK    38
#define TFT_CS      35
#define TFT_DC      36
#define TFT_RST     39

// Use FSPI peripheral on ESP32-S3
#define SPI_PORT    FSPI

// ST7735S 80×160 — no GRAM offset (COLSTART=0, ROWSTART=0).
// Content fills the full visible area; colour inversion is handled separately
// by TFT_INVERSION_ON below.
#define ST7735_BLACKTAB

// SPI speed — 40 MHz is the sweet-spot for ST7735S clones on ESP32-S3.
// (Datasheet max is 15.15 MHz but virtually all panels handle 40 MHz fine;
//  80 MHz may cause pixel corruption.)
#define SPI_FREQUENCY       40000000
#define SPI_READ_FREQUENCY   6000000

// Enable DMA: SPI pixel data is transferred by the DMA controller while
// the CPU is already preparing the next draw call.
#define USE_DMA_TO_TFT

// Fonts to compile in
#define LOAD_GLCD    // Font 1 — 6×8 pixel, default
#define LOAD_FONT2   // Font 2 — 7-segment style
#define SMOOTH_FONT
