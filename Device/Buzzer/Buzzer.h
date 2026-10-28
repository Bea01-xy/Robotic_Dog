/**
  ******************************************************************************
  * @file           : Buzzer.h
  * @brief          : Passive buzzer driver via TIM12_CH2 (PB15)
  * @author         : Ported from CtrBoard-H7_BUZZER
  ******************************************************************************
  * @attention
  *
  * Hardware: TIM12, Channel 2, PB15 (AF2)
  * Clock tree: SYSCLK=213.33MHz → HCLK=53.33MHz → APB1=26.67MHz
  *             TIM12 clock = APB1×2 = 53.33MHz (since APB1 prescaler != 1)
  *             PSC=15 → counter clock = 53.33MHz/16 ≈ 3.333MHz
  *
  * Usage:
  *   Buzzer_Init();
  *   Buzzer_PlayNote(NOTE_MIDDLE_C, 500);  // 中音 Do, 500ms
  *   Buzzer_SetFreq(440);                    // A4
  *   Buzzer_Stop();
  *   Buzzer_Demo();                          // 播放示例旋律
  *
  ******************************************************************************
  */

#ifndef __BUZZER_H__
#define __BUZZER_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/* ======================== 音高宏 ======================== */
/* 简谱 7 音 (do re mi fa so la ti) × 4 个八度               */
/* NOTE(oct, deg): oct: -1=低音(下一点), 0=中音, 1=高音, 2=倍高 */
/*                  deg: 1-7                                   */

#define NOTE_C(oct)  (((oct) == -1) ? 25479U : ((oct) == 0) ? 12739U : ((oct) == 1) ? 6369U : 3184U)
#define NOTE_D(oct)  (((oct) == -1) ? 22700U : ((oct) == 0) ? 11349U : ((oct) == 1) ? 5674U : 2837U)
#define NOTE_E(oct)  (((oct) == -1) ? 20224U : ((oct) == 0) ? 10112U : ((oct) == 1) ? 5056U : 2528U)
#define NOTE_F(oct)  (((oct) == -1) ? 19091U : ((oct) == 0) ?  9545U : ((oct) == 1) ? 4772U : 2386U)
#define NOTE_G(oct)  (((oct) == -1) ? 17005U : ((oct) == 0) ?  8502U : ((oct) == 1) ? 4251U : 2125U)
#define NOTE_A(oct)  (((oct) == -1) ? 15151U : ((oct) == 0) ?  7575U : ((oct) == 1) ? 3787U : 1893U)
#define NOTE_B(oct)  (((oct) == -1) ? 13499U : ((oct) == 0) ?  6749U : ((oct) == 1) ? 3374U : 1687U)

#define NOTE_REST     0      /* 休止符 */

/* ======================== 乐曲辅助宏 ======================== */
#define TEMPO(bpm)      (60000u / (bpm))       /* 四分音符时长(ms) */
#define HALF(t)         ((t) / 2)
#define QUARTER(t)      ((t) / 4)
#define DOTTED(t)       ((t) * 3 / 2)

/* ======================== 公有 API ======================== */

/**
  * @brief  初始化蜂鸣器 (TIM12, CH2, PB15)
  * @note   使能 TIM12 时钟、配置 GPIO、启动 PWM 输出
  */
void Buzzer_Init(void);

/**
  * @brief  设定 PWM 频率 (即音高)
  * @param  freq_hz  频率值 (Hz), 0 = 关闭输出
  */
void Buzzer_SetFreq(uint16_t freq_hz);

/**
  * @brief  设定音量 (占空比)
  * @param  percent  0~100 (0=静音, 100=最大)
  */
void Buzzer_SetVolume(uint8_t percent);

/**
  * @brief  停止蜂鸣器输出 (占空比置 0, 不停止定时器)
  */
void Buzzer_Stop(void);

/**
  * @brief  播放单个音符 (阻塞, 使用 osDelay)
  * @param  arr          频率对应 ARR 值 (NOTE_xxx 宏), NOTE_REST 为静音
  * @param  duration_ms  持续时间 (ms)
  */
void Buzzer_PlayNote(uint16_t arr, uint16_t duration_ms);

/**
  * @brief  播放示例旋律 (阻塞, 使用 osDelay)
  * @note   在 Detect_Task 启动时调用一次, 播放完毕后返回
  */
void Buzzer_Demo(void);

#ifdef __cplusplus
}
#endif

#endif /* __BUZZER_H__ */
