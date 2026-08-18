#include "test_main.h"

/* Include Begin */
// #include "bsp_SD.h"
// #include "sd_test.h"
// #include "sd_oled_test.h"
// #include "bsp_key.h"
// #include "playlist_test.h"
#include "app_test.h"
/* Include End */


void Test_Init(void){
    // BSP_SD_Init();
    // BSP_Key_Init();
    // SD_OLED_Test_Init();
    // Playlist_Test_Init();
    App_TestInit();
}

void Test_Run(void){
    // SD_Test_Run();
    // SD_OLED_Test_Run();
    // Playlist_Test_Run();
    App_TestRun();
}
