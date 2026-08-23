/**
  ******************************************************************************
  * @file    vs1003_test.c
  * @brief   VS1003 可听正弦波测试
  ******************************************************************************
  */

#include "vs1003_test.h"

#include "bsp_config.h"
#define LOG_MODULE "VS1003Test"
#include "bsp_key.h"
#include "bsp_vs1003.h"

/* 0x44 对应 VS1003 正弦测试约 1 kHz 的音调参数。 */
#define VS1003_TEST_TONE        0x44U
#define VS1003_TEST_DURATION_MS 3000U

static uint8_t vs1003_test_ready;

static void    VS1003_Test_PrintPins(const char *phase)
{
    LOG_DEBUG("GPIO", "%s: XRESET=%u XCS=%u XDCS=%u DREQ=%u", phase,
              (unsigned int)HAL_GPIO_ReadPin(VS1003_XRESET_GPIO_Port, VS1003_XRESET_Pin),
              (unsigned int)HAL_GPIO_ReadPin(VS1003_XCS_GPIO_Port, VS1003_XCS_Pin),
              (unsigned int)HAL_GPIO_ReadPin(VS1003_XDCS_GPIO_Port, VS1003_XDCS_Pin),
              (unsigned int)BSP_VS1003_IsReady());
}

static const char *VS1003_Test_StatusText(bsp_vs1003_status_t status)
{
    switch (status)
    {
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
    uint16_t            value = 0U;

    status = BSP_VS1003_ReadRegister(address, &value);
    if (status == BSP_VS1003_OK)
    {
        LOG_DEBUG("SCI", "%-6s (0x%02X) = 0x%04X", name, (unsigned int)address, (unsigned int)value);
    }
    else
    {
        LOG_DEBUG("SCI", "%-6s (0x%02X) read failed: status=%d (%s)", name, (unsigned int)address,
                  (int)status, VS1003_Test_StatusText(status));
    }
}

static void VS1003_Test_DumpRegisters(void)
{
    LOG_DEBUG("DIAG", "SCI register snapshot:");
    VS1003_Test_PrintRegister("MODE", BSP_VS1003_REG_MODE);
    VS1003_Test_PrintRegister("STATUS", BSP_VS1003_REG_STATUS);
    LOG_DEBUG("Message",
              "       STATUS[6:4]=0x3 identifies VS1003; STATUS[1:0] may change at runtime.");
    VS1003_Test_PrintRegister("CLOCKF", BSP_VS1003_REG_CLOCKF);
    VS1003_Test_PrintRegister("VOL", BSP_VS1003_REG_VOL);
}

static void VS1003_Test_PrintVerifyFailure(void)
{
    bsp_vs1003_verify_diag_t diag;

    if (BSP_VS1003_GetLastVerifyDiag(&diag) == 0U)
    {
        return;
    }

    LOG_DEBUG("VERIFY", "reg=0x%02X expected=0x%04X actual=0x%04X mask=0x%04X",
              (unsigned int)diag.address, (unsigned int)diag.expected, (unsigned int)diag.actual,
              (unsigned int)diag.mask);
}

static bsp_vs1003_status_t VS1003_Test_InitializeDevice(void)
{
    bsp_vs1003_status_t status;

    LOG_DEBUG("TEST 1A", "Hardware reset and DREQ check...");
    VS1003_Test_PrintPins("before reset");

    status = BSP_VS1003_HardwareReset();
    VS1003_Test_PrintPins("after reset");
    if (status != BSP_VS1003_OK)
    {
        vs1003_test_ready = 0U;
        LOG_DEBUG("FAIL", "Hardware reset: status=%d (%s)", (int)status,
                  VS1003_Test_StatusText(status));
        if (BSP_VS1003_IsReady() == 0U)
        {
            LOG_DEBUG("Message", "       DREQ stayed LOW after XRESET was released.");
            LOG_DEBUG("Message", "       No SCI register transaction has been attempted yet.");
            LOG_DEBUG(
                "Message",
                "       Measure PA8/XRESET and PB10/DREQ; check VS1003 power, GND and crystal.");
        }
        return status;
    }
    LOG_DEBUG("PASS", "DREQ became HIGH after hardware reset.");

    LOG_DEBUG("TEST 1B", "Initializing SCI registers...");
    /*
     * TEST 1A 已完成硬件复位。这里只配置 SCI，避免 BSP_VS1003_Init()
     * 再执行一次隐藏的硬件复位，使阶段日志与实际总线事务保持一致。
     */
    status = BSP_VS1003_InitAfterHardwareReset();
    VS1003_Test_PrintPins("after SCI init");
    if (status != BSP_VS1003_OK)
    {
        vs1003_test_ready = 0U;
        LOG_DEBUG("FAIL", "SCI initialization: status=%d (%s)", (int)status,
                  VS1003_Test_StatusText(status));
        if (status == BSP_VS1003_ERR_VERIFY)
        {
            VS1003_Test_PrintVerifyFailure();
            VS1003_Test_DumpRegisters();
        }
        LOG_DEBUG("Message", "       DREQ reset check passed; inspect XCS, SCK, MOSI and MISO with "
                             "a logic analyzer.");
        return status;
    }

    vs1003_test_ready = 1U;
    LOG_DEBUG("PASS", "SPI communication and register verification passed.");
    return BSP_VS1003_OK;
}

static void VS1003_Test_PlaySine(void)
{
    bsp_vs1003_status_t status;

    LOG_DEBUG("TEST 2", "Playing about 1 kHz sine tone for %lu ms...",
              (unsigned long)VS1003_TEST_DURATION_MS);
    LOG_DEBUG("Message", "         A clear steady tone should be audible now.");

    status = BSP_VS1003_SineTest(VS1003_TEST_TONE, VS1003_TEST_DURATION_MS);
    if (status == BSP_VS1003_OK)
    {
        vs1003_test_ready = 1U;
        LOG_DEBUG("PASS", "MCU completed the sine sequence and VS1003 restore.");
        LOG_DEBUG("Message", "       Confirm the test only if the tone was actually audible.");
    }
    else
    {
        vs1003_test_ready = 0U;
        LOG_DEBUG("FAIL", "Sine test: status=%d (%s)", (int)status, VS1003_Test_StatusText(status));
        if (status == BSP_VS1003_ERR_VERIFY)
        {
            LOG_DEBUG(
                "Message",
                "       Register verification failed during sine setup or post-test restore.");
            VS1003_Test_PrintVerifyFailure();
            VS1003_Test_DumpRegisters();
        }
    }

    LOG_DEBUG("Message", "Press and release OK to run the audible test again.");
}

void VS1003_Test_Init(void)
{
    LOG_DEBUG("Message", "");
    LOG_DEBUG("Banner", "VS1003 Audible Test Start");
    LOG_DEBUG("Message", "Use headphones or an active speaker at a safe volume.");

    if (VS1003_Test_InitializeDevice() == BSP_VS1003_OK)
    {
        VS1003_Test_PlaySine();
    }
    else
    {
        LOG_DEBUG("Message", "Press and release OK to retry initialization.");
    }
}

void VS1003_Test_Run(void)
{
    if (BSP_Key_GetEvent(KEY_OK) != KEY_EDGE_RELEASE)
    {
        return;
    }

    LOG_DEBUG("Banner", "VS1003 Audible Test Retry");

    if ((vs1003_test_ready == 0U) && (VS1003_Test_InitializeDevice() != BSP_VS1003_OK))
    {
        LOG_DEBUG("Message", "Press and release OK to retry initialization.");
        return;
    }

    VS1003_Test_PlaySine();
}
