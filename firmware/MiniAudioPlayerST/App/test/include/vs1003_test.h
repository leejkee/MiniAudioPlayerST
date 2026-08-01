/**
  ******************************************************************************
  * @file    vs1003_test.h
  * @brief   VS1003 可听正弦波测试
  * @note    VS1003_Test_Init() 会在初始化成功后自动播放一次测试音。
  *          主循环持续调用 VS1003_Test_Run()，释放 OK 键可重复测试。
  ******************************************************************************
  */

#ifndef __VS1003_TEST_H__
#define __VS1003_TEST_H__

#ifdef __cplusplus
extern "C" {
#endif

/**
  * @brief  初始化 VS1003 并播放一次约 1 kHz、持续 3 秒的测试音
  * @note   串口 PASS 表示 SPI/寄存器/正弦命令执行成功；
  *         最终声音输出是否正常仍需由耳机或有源音箱确认。
  */
void VS1003_Test_Init(void);

/**
  * @brief  处理测试按键
  * @note   需在主循环中持续调用。释放 OK 键后会重新播放测试音；
  *         若上次初始化失败，则先重试初始化。
  */
void VS1003_Test_Run(void);

#ifdef __cplusplus
}
#endif

#endif /* __VS1003_TEST_H__ */
