/**
  ******************************************************************************
  * @file    vs1003_test.c
  * @brief   VS1003 可听正弦波测试
  ******************************************************************************
  */

#include "vs1003_test.h"

#include "bsp_config.h"
#include "bsp_key.h"
#include "bsp_vs1003.h"

/* 0x44 对应 VS1003 正弦测试约 1 kHz 的音调参数。 */
#define VS1003_TEST_TONE             0x44U
#define VS1003_TEST_DURATION_MS      3000U

static uint8_t vs1003_test_ready;

static void VS1003_Test_PrintPins(const char *phase)
{
    BSP_DEBUG_PRINTF("[GPIO] %s: XRESET=%u XCS=%u XDCS=%u DREQ=%u\r\n",
                     phase,
                     (unsigned int)HAL_GPIO_ReadPin(VS1003_XRESET_GPIO_Port,
                                                   VS1003_XRESET_Pin),
                     (unsigned int)HAL_GPIO_ReadPin(VS1003_XCS_GPIO_Port,
                                                   VS1003_XCS_Pin),
                     (unsigned int)HAL_GPIO_ReadPin(VS1003_XDCS_GPIO_Port,
                                                   VS1003_XDCS_Pin),
                     (unsigned int)BSP_VS1003_IsReady());
}

static const char *VS1003_Test_StatusText(bsp_vs1003_status_t status)
{
    switch (status) {
    case BSP_VS1003_OK:
        return "OK";
    case BSP_VS1003_ERR_PARAM:
        return "invalid parameter";
    case BSP_VS1003_ERR_SPI:
        return "SPI transfer failed";
    case BSP_VS1003_ERR_TIMEOUT:
        return "DREQ/SPI timeout";
    case BSP_VS1003_ERR_VERIFY:
        return "register verify failed";
    case BSP_VS1003_ERR_NOT_READY:
        return "driver not ready";
    default:
        return "unknown error";
    }
}

static void VS1003_Test_PrintRegister(const char *name, uint8_t address)
{
    bsp_vs1003_status_t status;
    uint16_t value = 0U;

    status = BSP_VS1003_ReadRegister(address, &value);
    if (status == BSP_VS1003_OK) {
        BSP_DEBUG_PRINTF("[SCI] %-6s (0x%02X) = 0x%04X\r\n",
                         name,
                         (unsigned int)address,
                         (unsigned int)value);
    } else {
        BSP_DEBUG_PRINTF("[SCI] %-6s (0x%02X) read failed: status=%d (%s)\r\n",
                         name,
                         (unsigned int)address,
                         (int)status,
                         VS1003_Test_StatusText(status));
    }
}

static void VS1003_Test_DumpRegisters(void)
{
    BSP_DEBUG_PRINTF("[DIAG] SCI register snapshot:\r\n");
    VS1003_Test_PrintRegister("MODE", BSP_VS1003_REG_MODE);
    VS1003_Test_PrintRegister("STATUS", BSP_VS1003_REG_STATUS);
    BSP_DEBUG_PRINTF("       STATUS[6:4]=0x3 identifies VS1003; STATUS[1:0] may change at runtime.\r\n");
    VS1003_Test_PrintRegister("CLOCKF", BSP_VS1003_REG_CLOCKF);
    VS1003_Test_PrintRegister("VOL", BSP_VS1003_REG_VOL);
}

static void VS1003_Test_PrintVerifyFailure(void)
{
    bsp_vs1003_verify_diag_t diag;

    if (BSP_VS1003_GetLastVerifyDiag(&diag) == 0U) {
        return;
    }

    BSP_DEBUG_PRINTF("[VERIFY] reg=0x%02X expected=0x%04X actual=0x%04X mask=0x%04X\r\n",
                     (unsigned int)diag.address,
                     (unsigned int)diag.expected,
                     (unsigned int)diag.actual,
                     (unsigned int)diag.mask);
}

static bsp_vs1003_status_t VS1003_Test_InitializeDevice(void)
{
    bsp_vs1003_status_t status;

    BSP_DEBUG_PRINTF("[TEST 1A] Hardware reset and DREQ check...\r\n");
    VS1003_Test_PrintPins("before reset");

    status = BSP_VS1003_HardwareReset();
    VS1003_Test_PrintPins("after reset");
    if (status != BSP_VS1003_OK) {
        vs1003_test_ready = 0U;
        BSP_DEBUG_PRINTF("[FAIL] Hardware reset: status=%d (%s)\r\n",
                         (int)status,
                         VS1003_Test_StatusText(status));
        if (BSP_VS1003_IsReady() == 0U) {
            BSP_DEBUG_PRINTF("       DREQ stayed LOW after XRESET was released.\r\n");
            BSP_DEBUG_PRINTF("       No SCI register transaction has been attempted yet.\r\n");
            BSP_DEBUG_PRINTF("       Measure PA8/XRESET and PB10/DREQ; check VS1003 power, GND and crystal.\r\n");
        }
        return status;
    }
    BSP_DEBUG_PRINTF("[PASS] DREQ became HIGH after hardware reset.\r\n");

    BSP_DEBUG_PRINTF("[TEST 1B] Initializing SCI registers...\r\n");
    /*
     * TEST 1A 已完成硬件复位。这里只配置 SCI，避免 BSP_VS1003_Init()
     * 再执行一次隐藏的硬件复位，使阶段日志与实际总线事务保持一致。
     */
    status = BSP_VS1003_InitAfterHardwareReset();
    VS1003_Test_PrintPins("after SCI init");
    if (status != BSP_VS1003_OK) {
        vs1003_test_ready = 0U;
        BSP_DEBUG_PRINTF("[FAIL] SCI initialization: status=%d (%s)\r\n",
                         (int)status,
                         VS1003_Test_StatusText(status));
        if (status == BSP_VS1003_ERR_VERIFY) {
            VS1003_Test_PrintVerifyFailure();
            VS1003_Test_DumpRegisters();
        }
        BSP_DEBUG_PRINTF("       DREQ reset check passed; inspect XCS, SCK, MOSI and MISO with a logic analyzer.\r\n");
        return status;
    }

    vs1003_test_ready = 1U;
    BSP_DEBUG_PRINTF("[PASS] SPI communication and register verification passed.\r\n");
    return BSP_VS1003_OK;
}

static void VS1003_Test_PlaySine(void)
{
    bsp_vs1003_status_t status;

    BSP_DEBUG_PRINTF("[TEST 2] Playing about 1 kHz sine tone for %lu ms...\r\n",
                     (unsigned long)VS1003_TEST_DURATION_MS);
    BSP_DEBUG_PRINTF("         A clear steady tone should be audible now.\r\n");

    status = BSP_VS1003_SineTest(VS1003_TEST_TONE,
                                 VS1003_TEST_DURATION_MS);
    if (status == BSP_VS1003_OK) {
        vs1003_test_ready = 1U;
        BSP_DEBUG_PRINTF("[PASS] MCU completed the sine sequence and VS1003 restore.\r\n");
        BSP_DEBUG_PRINTF("       Confirm the test only if the tone was actually audible.\r\n");
    } else {
        vs1003_test_ready = 0U;
        BSP_DEBUG_PRINTF("[FAIL] Sine test: status=%d (%s)\r\n",
                         (int)status,
                         VS1003_Test_StatusText(status));
        if (status == BSP_VS1003_ERR_VERIFY) {
            BSP_DEBUG_PRINTF("       Register verification failed during sine setup or post-test restore.\r\n");
            VS1003_Test_PrintVerifyFailure();
            VS1003_Test_DumpRegisters();
        }
    }

    BSP_DEBUG_PRINTF("Press and release OK to run the audible test again.\r\n");
}

void VS1003_Test_Init(void)
{
    BSP_DEBUG_PRINTF("\r\n");
    BSP_DEBUG_PRINTF("========== VS1003 Audible Test Start ==========\r\n");
    BSP_DEBUG_PRINTF("Use headphones or an active speaker at a safe volume.\r\n");

    if (VS1003_Test_InitializeDevice() == BSP_VS1003_OK) {
        VS1003_Test_PlaySine();
    } else {
        BSP_DEBUG_PRINTF("Press and release OK to retry initialization.\r\n");
    }
}

void VS1003_Test_Run(void)
{
    if (BSP_Key_GetEvent(KEY_OK) != KEY_EDGE_RELEASE) {
        return;
    }

    BSP_DEBUG_PRINTF("\r\n========== VS1003 Audible Test Retry ==========\r\n");

    if ((vs1003_test_ready == 0U)
        && (VS1003_Test_InitializeDevice() != BSP_VS1003_OK)) {
        BSP_DEBUG_PRINTF("Press and release OK to retry initialization.\r\n");
        return;
    }

    VS1003_Test_PlaySine();
}
