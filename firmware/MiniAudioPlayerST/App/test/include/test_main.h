#ifndef __TEST_MAIN_H
#define __TEST_MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Set to 1 to run Test_Main, or 0 to run the application. */
#define TEST_MAIN_ENABLED 0

/* Test Inlcude Begin */
#include "main.h"
/* Test Inlcude End */


/* Test Defination Begin */
void Test_Init(void);

void Test_Run(void);

/* Test Defination End */

#ifdef __cplusplus
}
#endif

#endif
