/**
  ******************************************************************************
  * @file           : WS2812.c
  * @brief          : WS2812 LED driver via SPI6 (PA5/PA7)
  * @author         : Ported from CtrBoard-H7_WS2812
  ******************************************************************************
  *
  * 时钟推导:
  *   HSE = 8 MHz
  *   SPI6 内核时钟源 = HSE (通过 RCC_D2CCIP2R.SPI6SEL)
  *   预分频 /4          → SPI6_CLK = 8 MHz / 4 = 2 MHz
  *
  * WS2812 时序:
  *   每 WS2812 位用 8 个 SPI 位编码:
  *     0: 0xC0>>1 = 0x60   (0110_0000)
  *     1: 0xF0>>1 = 0x78   (0111_1000)
  *   每灯 24 字节 (G-R-B 各 8 字节)
  *   复位: >280 μs 低电平 → 100 字节 0x00 @ 2 MHz = 400 μs
  *
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "WS2812.h"
#include "main.h"            /* HAL, Error_Handler */
#include "cmsis_os.h"        /* osDelay */

/* Private defines -----------------------------------------------------------*/

/* WS2812 编码电平 (高位在前, 每 WS2812 位映射为 8 SPI 位) */
#define WS2812_LOW_LEVEL    0xC0     /* 0 码 */
#define WS2812_HIGH_LEVEL   0xF0     /* 1 码 */

/* 传输缓冲区大小: 每灯 24 编码字节 + 100 复位字节 */
#define TX_BUF_SIZE         (WS2812_LED_NUM * 24 + 100)

/* Private variables ---------------------------------------------------------*/
static SPI_HandleTypeDef hspi6;              /* SPI6 句柄 — 自包含 */
static uint8_t           ws2812_inited = 0;  /* 初始化标志 */

/* LED RGB 缓存 */
static uint8_t led_r[WS2812_LED_NUM];
static uint8_t led_g[WS2812_LED_NUM];
static uint8_t led_b[WS2812_LED_NUM];

/* ======================== 私有函数 ======================== */

/**
  * @brief  将单灯 RGB 编码为 24 字节 SPI 数据 (G-R-B 顺序)
  * @param  r, g, b  颜色分量 (0-255)
  * @param  buf      输出缓冲区 (至少 24 字节)
  */
static void ws2812_encode(uint8_t r, uint8_t g, uint8_t b, uint8_t *buf)
{
    for (int i = 0; i < 8; i++)
    {
        buf[7  - i] = (((g >> i) & 0x01) ? WS2812_HIGH_LEVEL : WS2812_LOW_LEVEL) >> 1;
        buf[15 - i] = (((r >> i) & 0x01) ? WS2812_HIGH_LEVEL : WS2812_LOW_LEVEL) >> 1;
        buf[23 - i] = (((b >> i) & 0x01) ? WS2812_HIGH_LEVEL : WS2812_LOW_LEVEL) >> 1;
    }
}

/* ======================== 公有 API ======================== */

/**
  * @brief  初始化 WS2812 (SPI6, PA5/PA7)
  * @note   自包含, 不依赖 CubeMX 生成的引脚或外设配置
  *         时钟路径: HSE(8MHz) → SPI6 内核时钟, 预分频 /4 → 2MHz
  */
void WS2812_Init(void)
{
    GPIO_InitTypeDef        gpio       = {0};
    RCC_PeriphCLKInitTypeDef periph_clk = {0};

    if (ws2812_inited) return;

    /* ---- 1. SPI6 内核时钟源: HSE (8 MHz) ---- */
    periph_clk.PeriphClockSelection = RCC_PERIPHCLK_SPI6;
    periph_clk.Spi6ClockSelection   = RCC_SPI6CLKSOURCE_HSE;
    if (HAL_RCCEx_PeriphCLKConfig(&periph_clk) != HAL_OK) {
        Error_Handler();
    }

    /* ---- 2. 外设时钟使能 ---- */
    __HAL_RCC_SPI6_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();

    /* ---- 3. GPIO: PA5(SCK), PA7(MOSI) → AF8 ---- */
    gpio.Pin       = GPIO_PIN_5 | GPIO_PIN_7;
    gpio.Mode      = GPIO_MODE_AF_PP;
    gpio.Pull      = GPIO_NOPULL;
    gpio.Speed     = GPIO_SPEED_FREQ_MEDIUM;
    gpio.Alternate = GPIO_AF8_SPI6;
    HAL_GPIO_Init(GPIOA, &gpio);

    /* ---- 4. SPI6 基础配置 ---- */
    hspi6.Instance               = SPI6;
    hspi6.Init.Mode              = SPI_MODE_MASTER;
    hspi6.Init.Direction         = SPI_DIRECTION_2LINES_TXONLY;
    hspi6.Init.DataSize          = SPI_DATASIZE_8BIT;
    hspi6.Init.CLKPolarity       = SPI_POLARITY_LOW;
    hspi6.Init.CLKPhase          = SPI_PHASE_1EDGE;
    hspi6.Init.NSS               = SPI_NSS_SOFT;
    hspi6.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_4;  /* HSE/4 = 2 MHz */
    hspi6.Init.FirstBit          = SPI_FIRSTBIT_MSB;
    hspi6.Init.TIMode            = SPI_TIMODE_DISABLE;
    hspi6.Init.CRCCalculation    = SPI_CRCCALCULATION_DISABLE;
    hspi6.Init.CRCPolynomial     = 7;
    hspi6.Init.CRCLength     = SPI_CRC_LENGTH_8BIT;
    hspi6.Init.NSSPMode      = SPI_NSS_PULSE_DISABLE;
    if (HAL_SPI_Init(&hspi6) != HAL_OK) {
        Error_Handler();
    }

    ws2812_inited = 1;
}

/**
  * @brief  控制单盏灯 (立即发送, 阻塞)
  * @param  r, g, b  颜色分量 (0-255)
  * @note   与参考项目接口一致, 仅控制单灯, 不操作缓存
  */
void WS2812_Ctrl(uint8_t r, uint8_t g, uint8_t b)
{
    uint8_t txbuf[24];
    uint8_t zero = 0;
    int i;

    if (!ws2812_inited) return;

    /* 发送 1 字节零 (刷新准备/同步) */
    HAL_SPI_Transmit(&hspi6, &zero, 0, 0xFFFF);
    while (hspi6.State != HAL_SPI_STATE_READY);

    /* 编码并发送 24 字节 */
    ws2812_encode(r, g, b, txbuf);
    HAL_SPI_Transmit(&hspi6, txbuf, 24, 0xFFFF);
    while (hspi6.State != HAL_SPI_STATE_READY);

    /* 100 字节复位低电平 (>280 μs) */
    for (i = 0; i < 100; i++) {
        HAL_SPI_Transmit(&hspi6, &zero, 1, 0xFFFF);
    }
}

/**
  * @brief  设置指定灯珠的 RGB 值 (存入缓存)
  * @param  index  灯珠序号 (0 ~ WS2812_LED_NUM-1)
  * @param  r, g, b  颜色分量 (0-255)
  * @note   修改缓存后需调用 WS2812_Update() 才会生效
  */
void WS2812_SetLED(uint16_t index, uint8_t r, uint8_t g, uint8_t b)
{
    if (!ws2812_inited || index >= WS2812_LED_NUM) return;

    led_r[index] = r;
    led_g[index] = g;
    led_b[index] = b;
}

/**
  * @brief  将缓存中所有 LED 数据一次性发送到灯带 (阻塞)
  * @note   发送完毕自动追加 100 字节复位信号
  */
void WS2812_Update(void)
{
    uint8_t txbuf[TX_BUF_SIZE];
    uint16_t i;

    if (!ws2812_inited) return;

    /* 编码所有 LED */
    for (i = 0; i < WS2812_LED_NUM; i++) {
        ws2812_encode(led_r[i], led_g[i], led_b[i], &txbuf[i * 24]);
    }

    /* 复位信号 (全零) */
    for (i = WS2812_LED_NUM * 24; i < TX_BUF_SIZE; i++) {
        txbuf[i] = 0;
    }

    /* 一次性发送 */
    HAL_SPI_Transmit(&hspi6, txbuf, TX_BUF_SIZE, 0xFFFF);
}

/**
  * @brief  WS2812 演示 — 呼吸 + 流水灯 (阻塞)
  * @note   在 Detect_Task 启动时调用一次, 播放完毕后返回
  */
void WS2812_Demo(void)
{
    uint16_t i, step;

    if (!ws2812_inited) {
        WS2812_Init();
    }

    /* ======== Phase 1: 呼吸 (三区渐变) ======== */
    for (step = 0; step < 256; step++)
    {
        uint8_t brightness;

        /* 三角波: 0 → 255 → 0 */
        if (step < 128)
            brightness = (uint8_t)(step * 2);
        else
            brightness = (uint8_t)((255 - step) * 2);

        /* 分区颜色: 红 / 绿 / 蓝 */
        for (i = 0; i < WS2812_LED_NUM; i++)
        {
            if (i < WS2812_LED_NUM / 3)
                WS2812_SetLED(i, brightness, 0, 0);                 /* 红区 */
            else if (i < WS2812_LED_NUM * 2 / 3)
                WS2812_SetLED(i, 0, brightness, 0);                 /* 绿区 */
            else
                WS2812_SetLED(i, 0, 0, brightness);                 /* 蓝区 */
        }
        WS2812_Update();
        osDelay(10);
    }

    /* ======== Phase 2: 流水灯 (单灯 + 拖尾) ======== */
    for (step = 0; step < (uint16_t)(WS2812_LED_NUM * 4); step++)
    {
        /* 清除所有 */
        for (i = 0; i < WS2812_LED_NUM; i++) {
            WS2812_SetLED(i, 0, 0, 0);
        }

        /* 主灯 — 淡蓝色 */
        i = step % WS2812_LED_NUM;
        WS2812_SetLED(i, 50, 100, 200);

        /* 拖尾 */
        if (i > 0)     WS2812_SetLED(i - 1, 20, 40, 80);
        if (i > 1)     WS2812_SetLED(i - 2,  5, 10, 20);

        WS2812_Update();
        osDelay(80);
    }

    /* 最终全部熄灭 */
    for (i = 0; i < WS2812_LED_NUM; i++) {
        WS2812_SetLED(i, 0, 0, 0);
    }
    WS2812_Update();
}
