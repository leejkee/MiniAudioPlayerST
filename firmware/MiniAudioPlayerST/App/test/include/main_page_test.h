/**
  ******************************************************************************
  * @file    main_page_test.h
  * @brief   主界面播放器联合测试
  ******************************************************************************
  */

#ifndef MAIN_PAGE_TEST_H
#define MAIN_PAGE_TEST_H

#ifdef __cplusplus
extern "C" {
#endif

/**
  * @brief  初始化 OLED、按键、播放器并进入主界面
  */
void MainPage_Test_Init(void);

/**
  * @brief  驱动音频和 UI，并通过串口输出主界面核对信息
  */
void MainPage_Test_Run(void);

#ifdef __cplusplus
}
#endif

#endif /* MAIN_PAGE_TEST_H */
