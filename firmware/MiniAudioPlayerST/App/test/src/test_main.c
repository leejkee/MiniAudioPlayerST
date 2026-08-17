#include "test_main.h"

/* Include Begin */
// #include "bsp_SD.h"
// #include "sd_test.h"
#include "sd_oled_test.h"
#include "bsp_key.h"
/* Include End */


void Test_Init(void){
    // BSP_SD_Init();
    BSP_Key_Init();
    SD_OLED_Test_Init();
}

void Test_Run(void){
    // SD_Test_Run();
    SD_OLED_Test_Run();
}
