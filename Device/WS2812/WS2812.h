/**
  ******************************************************************************
  * @file           : WS2812.h
  * @brief          : WS2812 LED driver via SPI6 (PA5/PA7)
  * @author         : Ported from CtrBoard-H7_WS2812
  ******************************************************************************
  * @attention
  *
  * Hardware: SPI6, PA5(SCK), PA7(MOSI), AF8
  * Clock tree: HSE(8MHz) → SPI6 kernel clock, prescaler /4 → 2MHz SPI clk
  *
  * Usage:
  *   WS2812_Init();
  *   WS2812_SetLED(0, 255, 0, 0);      // 第 0 灯红色
  *   WS2812_SetLED(1, 0, 255, 0);      // 第 1 灯绿色
  *   WS2812_Update();                   // 批量发送
  *   WS2812_Ctrl(128, 128, 255);        // 单灯立即发送
  *   WS2812_Demo();                     // 呼吸+流水灯演示
  *
  ******************************************************************************
  */

#ifndef __WS2812_H__
#define __WS2812_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/* ======================== 灯珠数量 ======================== */
/* 可通过编译选项覆盖, 例如: -DWS2812_LED_NUM=16 */
#ifndef WS2812_LED_NUM
#define WS2812_LED_NUM  3
#endif

/* ======================== 公有 API ======================== */

/**
  * @brief  初始化 WS2812 (SPI6, PA5/PA7)
  * @note   时钟源 HSE, 预分频 /4 → 2 MHz
  *         自包含, 不依赖 CubeMX 生成的代码
  */
void WS2812_Init(void);

/**
  * @brief  控制单盏灯 (立即发送, 阻塞)
  * @param  r/g/b  颜色分量 (0-255)
  * @note   与参考项目接口一致, 仅控制单灯, 不操作缓存
  */
void WS2812_Ctrl(uint8_t r, uint8_t g, uint8_t b);

/**
  * @brief  设置指定灯珠的 RGB 值 (存入缓存)
  * @param  index  灯珠序号 (0 ~ WS2812_LED_NUM-1)
  * @param  r/g/b  颜色分量 (0-255)
  * @note   修改缓存后需调用 WS2812_Update() 才会生效
  */
void WS2812_SetLED(uint16_t index, uint8_t r, uint8_t g, uint8_t b);

/**
  * @brief  将缓存中所有 LED 数据一次性发送到灯带 (阻塞)
  * @note   发送完毕自动追加 100 字节复位信号
  */
void WS2812_Update(void);

/**
  * @brief  WS2812 演示 — 呼吸 + 流水灯 (阻塞)
  * @note   在 Detect_Task 启动时调用一次, 播放完毕后返回
  */
void WS2812_Demo(void);

#ifdef __cplusplus
}
#endif

#endif /* __WS2812_H__ */
