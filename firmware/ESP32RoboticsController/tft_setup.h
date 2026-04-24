// =============================================================================
// TFT_eSPI User Setup — ESP32-S3 Robotics Controller
// ST7735S 0.96" 160×80 TFT  |  Custom PCB by Avery Izatt
// =============================================================================
// Pin mapping from config.h:
//   MOSI = 37   SCLK = 38   CS = 35   DC = 36   RST = 39
// =============================================================================

// --- Driver selection --------------------------------------------------------
#define ST7735_DRIVER

// --- Panel dimensions --------------------------------------------------------
#define TFT_WIDTH   80
#define TFT_HEIGHT  160

// --- SPI pins ----------------------------------------------------------------
#define TFT_MOSI    37
#define TFT_SCLK    38
#define TFT_CS      35
#define TFT_DC      36
#define TFT_RST     39
#define TFT_MISO    -1

// --- SPI speed ---------------------------------------------------------------
// ST7735S write-cycle minimum 66ns → max ~15 MHz.
// At 3.3 V on short PCB traces 20–27 MHz is usually stable.
#define SPI_FREQUENCY         27000000
#define SPI_READ_FREQUENCY    6000000

// --- Fonts -------------------------------------------------------------------
#define LOAD_GLCD   // built-in 5×7 font
#define LOAD_FONT2  // small 16px
#define LOAD_FONT4  // medium 26px

// --- Misc --------------------------------------------------------------------
#define SMOOTH_FONT            // enable anti-aliased font rendering

// ESP-IDF's REG_SPI_BASE(i) returns 0 for i<2, but TFT_eSPI defaults
// SPI_PORT to FSPI (=1) on ESP32-S3.  Defining USE_FSPI_PORT forces
// SPI_PORT to 2, which matches ESP-IDF's register base macro.
#define USE_FSPI_PORT

// --- ST7735 initialisation variant -------------------------------------------
// Mini 160×80 0.96" module (black tab / green tab offset)
#define ST7735_GREENTAB160x80

// The panel requires colour inversion
#define TFT_INVERSION_ON

// --- DMA support -------------------------------------------------------------
// ESP32-S3 DMA is auto-configured by TFT_eSPI (SPI_DMA_CH_AUTO).
// Do NOT define ESP32_DMA or DMA_CHANNEL here — the library handles it.
