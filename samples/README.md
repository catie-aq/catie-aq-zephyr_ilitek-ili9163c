.. 6tron :code-sample:: display
   :name: Display
   :relevant-api: display_interface

   Draw basic rectangles from zephyr display samples [https://github.com/zephyrproject-rtos/zephyr/tree/main/samples/drivers/display].
# Overview

This sample application demonstrates the usage of the ILI9163C display from ILITEK.

This example:

- Draws rectangles onto the display in clockwise using the CATIE color palette :
    - Top Left: Orange
    - Top Right: Blue
    - Bottom Right: Green
    - Bottom Left: Gray
- Uses PWM to control the backlight brightness.

> [!NOTE]
> Ensure the display and PWM hardware configurations match the setup below for correct operation.

# Requirements

- **Hardware:**
    - Any compatible Zest Core board.
    - Board supporting I2C communication.
- **Configuration Options:**
    - Set `CONFIG_DISPLAY=y` in `prj.conf` to use the DISPLAY API.
    - Set `CONFIG_PWM=y` in `prj.conf` to use the PWM API.

# References

- [ER-TFT018-2 LCD](https://www.buydisplay.com/download/manual/ER-TFT018-2_Datasheet.pdf)
- [ILI9163C Datasheet](https://www.buydisplay.com/download/ic/ILI9163.pdf)

# Building and Running

```shell
cd <driver_directory>
west build -p always -b <BOARD> samples/ -- -D DTC_OVERLAY_FILE=boards/sixtron_bus.overlay
west flash
```
