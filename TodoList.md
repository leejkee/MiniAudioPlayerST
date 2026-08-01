# Todo List

## VS1003

- [ ] 将 VS1003 诊断功能从正式播放驱动中拆分。
  - 将 `BSP_VS1003_SineTest()` 以及后续的 SCI 稳定性、存储器等测试迁移到独立的 BSP 诊断模块，例如 `BSP/test/bsp_vs1003_diag.c`。
  - 保持 SPI、XCS、XDCS、DREQ 和复位等底层硬件操作封装在 BSP 内，不让 `App/test` 直接操作硬件。
  - 增加类似 `BSP_VS1003_ENABLE_DIAGNOSTICS` 的条件编译选项，使正式生产固件可以排除诊断代码。
  - 在 VS1003 硬件验证完成并进入正式功能集成阶段后实施。
