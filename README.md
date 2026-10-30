USB2Sun
=======
While I've found many projects for connecting the Sun serial based keyboards to a modern
machine via USB, I've never found anything to connect a USB keyboard onto a classical
Sun workstation.

This project allows you to connect a USB keyboard and mouse to a Sun workstation, using a Raspberry Pico as a bridge.

I've used two SN74LVC244AN as level shifters, one for 3.3V to 5V (from Pico to Sun),
and one for 5V to 3.3V (from Sun to Pico). There are probably smaller shifters than
the octal shifter I found, but I couldn't find any.

The Pico board is powered by the Sun workstation's 5V supply, taken from the 8 pin
mini-DIN keyboard port (pins 3 and 8), so no separate power supply is needed. The same
5V rail also powers the USB keyboard and mouse.

This project is currently targetting Pico SDK 2.2.0, and the corresponding TinyUSB version.

The USB host runs on two GPIOs (D+ on GP15, D- on GP16) using
[Pico-PIO-USB](https://github.com/sekigon-gonnoc/Pico-PIO-USB), so the Pico's own
micro-USB connector is left free and no OTG adapter is needed. The native port can
still be used instead by building with `-DUSB2SUN_PIO_USB=OFF`.

Features
--------
* Emulate the left keyboard function keys by entering LeftShift+LeftControl and the corresponding key

  | PC  | Function   |
  |-----|------------|
  | F1  | Stop       |
  | F2  | Again      |
  | 1   | Props      |
  | 2   | Undo       |
  | Q   | Front      |
  | W   | Copy       |
  | A   | Open       |
  | S   | Paste      |
  | Z   | Find       |
  | X   | Cut        |

  Breaking into OpenFirmware would be LeftShift+LeftControl+F1+A

Missing Features
----------------
* Keyboard power on. Shorting minidin pin 7 to 5V rail powers on the system, however, the 5V rail is unpowered when the system is off - so the Pico is also unpowered in this state.
* LED indicators (num - caps and scroll lock).
* Switching layouts. PC or Unix layout and international layout (the emulated keyboard responds US for now).

Build
-----
Make sure the Pico SDK has been checked out, as detailed by the Pi foundation, and
fetch the Pico-PIO-USB submodule (or clone this repository with `--recursive`).

    git submodule update --init
    mkdir build && cd build
    PICO_SDK_PATH=../../pico-sdk cmake ..
    make

Pico-PIO-USB requires the GNU Arm toolchain (`arm-none-eabi-gcc`); it does not build
with clang.

Build options (pass to cmake as `-DNAME=VALUE`):

| Option                        | Default | Description                                               |
|-------------------------------|---------|-----------------------------------------------------------|
| `USB2SUN_PIO_USB`             | `ON`    | USB host on GPIOs via PIO. `OFF` uses the native USB port |
| `USB2SUN_PIO_USB_DP_PIN`      | `15`    | GPIO for USB D+. D- is always the next GPIO (D+ + 1)      |
| `USB2SUN_PIO_USB_VBUSEN_PIN`  | `22`    | GPIO driven high at boot, for an optional VBUS switch     |

For example, to use GP2/GP3 for USB instead of GP15/GP16:

    PICO_SDK_PATH=../../pico-sdk cmake -DUSB2SUN_PIO_USB_DP_PIN=2 ..

To build the original variant that uses the Pico's micro-USB port with an OTG adapter:

    PICO_SDK_PATH=../../pico-sdk cmake -DUSB2SUN_PIO_USB=OFF ..

Flashing
--------
Hold BOOTSEL while plugging the Pico's micro-USB into a PC and copy `usb2sun.uf2` onto
the drive that appears. Because the keyboard no longer uses the micro-USB port, this
works even while the Pico is wired into the Sun (see the Power section for why).

If you're flashing with a Raspberry Pi over SWD, use

    openocd -f interface/raspberrypi-swd.cfg -f target/rp2040.cfg -c "program usb2sun.elf verify reset exit"

Usage
-----
Connect a USB mouse and keyboard to the USB-A socket wired to GP15/GP16.  Wireless combo sets will work best, as they do not require a hub.  A hub with up to 4 devices behind it is supported, but keep the total current draw low as everything is powered from the Sun's keyboard port.

The project can be either plugged directly into the keyboard port on the workstation, or daisy-chained to a Sun keyboard.  If daisy-chaining to the keyboard, do not connect the UART1 pins (Pico pins 6 and 7), only connect UART0 TX (Pico pin 1) to the Mouse TX pin on the minidin.

Status LED
----------
The Pico's on-board LED shows the USB state:

| Pattern                              | Meaning                                        |
|--------------------------------------|------------------------------------------------|
| Slow blink, 1 s on / 1 s off         | No keyboard or mouse detected                  |
| 3 quick blinks                       | Keyboard connected or disconnected             |
| 5 slower blinks                      | Mouse connected or disconnected                |
| Short flash every 2 s                | Running normally, at least one device attached |

If neither a keyboard nor a mouse is connected once 15 s have passed since power-up, the
Pico reboots via the watchdog and tries again.  This recovers from the occasional failed
USB enumeration.  It also means unplugging the last USB device will reboot the Pico,
which is harmless; it comes straight back and waits for a device.

Troubleshooting
---------------
* **LED keeps slow-blinking and the Pico reboots every 15 s.**  The USB device isn't
  enumerating.  Check D+ and D- aren't swapped (D+ is GP15 on Pico pin 20, D- is GP16
  on Pico pin 21; note GPIO numbers differ from board pin numbers), the USB-A
  ground is tied to Pico GND, and VBUS at the socket is close to 5V.  Try a different
  keyboard; some wireless dongles need a hub reset and work on the second attempt.
* **Mouse button stuck down or the pointer only jitters.**  Some mice (PixArt based
  ones in particular) accept the boot-protocol request and then keep sending their
  full report-protocol reports anyway.  USB2Sun parses each interface's report
  descriptor (`hid_layout.c`) and decodes such reports by field position, falling
  back to the boot layout when a report does not match the descriptor.  If a mouse
  still misbehaves, capture its reports with the piousb_debug console (`reports on`)
  and check which decoder they hit (`hid`).
* **Keys drop or repeat.**  PIO USB is bit-banged, so noisy or long D+/D- wires hurt.
  Keep them under 10 cm and equal length, add the optional 22 Ω series resistors, and
  route them away from the 5V and serial lines.
* **Works on a PC but not from the Sun.**  Check the 5V from the mini-DIN is reaching
  VSYS (pin 39).  Some Sun keyboard ports supply less current than expected; try a
  plain wired keyboard without a hub.
* **Want to rule out the PIO port.**  Build with `-DUSB2SUN_PIO_USB=OFF` and connect
  the keyboard via an OTG adapter on the micro-USB port to compare.

Parts required
--------------
* Raspberry Pico
* 8 pin minidin male cable (I used
https://no.rs-online.com/web/p/din-cable-assemblies/0463518)
* 2 SN74LVC244AN
* USB-A female socket or breakout board (or an OTG adapter, if building with `-DUSB2SUN_PIO_USB=OFF`)
* Optional: 2 x 22 Ω resistors for the USB data lines
* Optional: 1 Schottky diode (e.g. 1N5817) between the mini-DIN 5V and the Pico's VSYS

Wiring
------
Raspberry Pico

     UART0 TX   * Pin  1-----|USB|------Pin 40 *   VBUS (micro-USB only, leave unconnected)
                * Pin  2     `‾‾‾´      Pin 39 *   VSYS <- 5V from mini-DIN pin 3/8
                * Pin  3                Pin 38 *   GND  <- GND from mini-DIN pin 1/2
                * Pin  4                Pin 37 * 
                * Pin  5                Pin 36 *   3V3
     UART1 TX   * Pin  6                Pin 35 * 
     UART1 RX   * Pin  7                Pin 34 * 
                * Pin  8                Pin 33 * 
                * Pin  9   .--------.   Pin 32 * 
                * Pin 10   |        |   Pin 31 * 
                * Pin 11   | RP2040 |   Pin 30 * 
                * Pin 12   `--------´   Pin 29 *   GP22 USB VBUS EN (optional)
                * Pin 13                Pin 28 *   GND
                * Pin 14                Pin 27 * 
                * Pin 15                Pin 26 * 
                * Pin 16                Pin 25 * 
                * Pin 17       π        Pin 24 * 
     GND (USB)  * Pin 18                Pin 23 *   GND
                * Pin 19                Pin 22 *
     D+ GP15    * Pin 20----------------Pin 21 *   GP16 USB D-

The Pico's own micro-USB connector (top) is not used for the keyboard; the keyboard
and mouse plug into a USB-A socket wired to GP15/GP16 (Pico pins 20/21).

Power

                mini-DIN pin 3/8 (5V) ---+--- Pico pin 39 (VSYS)
                                         +--- USB-A pin 1 (VBUS)
                                         +--- SN74LVC244AN 3V3-to-5V shifter, pin 20 (VCC)

                mini-DIN pin 1/2 (GND) --+--- Pico pin 38 (GND)
                                         +--- USB-A pin 4 (GND)
                                         +--- both SN74LVC244AN GND pins

Feed the Sun's 5V into VSYS (pin 39), not VBUS (pin 40). VSYS is the Pico's external
power input and sits behind the on-board Schottky diode, so the micro-USB port can
still be plugged into a PC for flashing while the Pico is wired to the Sun without
shorting the two 5V supplies together. VBUS (pin 40) is only live when the micro-USB
is plugged in, so do not take the keyboard's power from it.

Optionally put a Schottky diode (e.g. 1N5817) in series between the mini-DIN 5V and
VSYS. This stops a PC on the micro-USB from back-feeding 5V into a powered-off Sun.
If you add it, keep the USB-A VBUS and the level shifter supply on the mini-DIN side
of the diode so the keyboard still gets a full 5V.

The whole bridge, including a typical wired keyboard and mouse, draws well under
500 mA from the Sun's keyboard port. Avoid hubs with several bus-powered devices.

USB-A socket (keyboard/mouse), looking into the socket

     .-------------------------.
     |  _____________________  |
     | |  4     3     2     1 | |
     | | GND    D+    D-   VBUS| |
     | |_____________________| |
     `-------------------------´

| USB-A pin | Signal | Wire colour | Connect to                        |
|-----------|--------|-------------|-----------------------------------|
| 1         | VBUS   | Red         | 5V rail from mini-DIN pin 3/8     |
| 2         | D-     | White       | Pico pin 21 (GP16)                |
| 3         | D+     | Green       | Pico pin 20 (GP15)                |
| 4         | GND    | Black       | Pico pin 18 (GND)                 |

Keep the D+/D- wires short and of equal length.  Pico-PIO-USB works with the GPIOs
wired directly to the socket; a 22 Ω series resistor in each data line is optional.
USB data lines are 3.3V signalling, so no level shifter is needed on D+/D-.  VBUS for
the keyboard comes straight from the Sun's 5V rail.  GP22 (Pico pin 29) is driven
high at boot and can switch a high-side VBUS switch if you want one; otherwise leave
it unconnected.

5V to 3V3 level shifter

          GND   * Pin  1----------------Pin 20 *   3V3
       KBD RX   * Pin  2                Pin 19 *
                * Pin  3                Pin 18 *   UART1 RX
                * Pin  4                Pin 17 *
                * Pin  5  SN74LVC244AN  Pin 16 *
                * Pin  6                Pin 15 *
                * Pin  7                Pin 14 *
                * Pin  8                Pin 13 *
                * Pin  9                Pin 12 *
          GND   * Pin 10----------------Pin 11 *

3V3 to 5V level shifter

          GND   * Pin  1----------------Pin 20 *   5V
     UART1 TX   * Pin  2                Pin 19 *
                * Pin  3                Pin 18 *   KBD TX
     UART0 TX   * Pin  4                Pin 17 *
                * Pin  5  SN74LVC244AN  Pin 16 *   MOUSE TX
                * Pin  6                Pin 15 *
                * Pin  7                Pin 14 *
                * Pin  8                Pin 13 *
                * Pin  9                Pin 12 *
          GND   * Pin 10----------------Pin 11 *

MiniDin-8 plug

            , - ~._.~ - ,
        , '               ' ,
      ,     6     7     8     ,
     ,                         ,
    ,                           ,
    ,    3    4           5     ,
    ,                           ,
     ,                          ,
      ,_      1      2       _,
        `-.              .-´
          ' - , _ _ _ , -'

| Pin | Function   |
|-----|------------|
| 1   | GND        |
| 2   | GND        |
| 3   | 5V         |
| 4   | MOUSE TX   |
| 5   | KBD RX     |
| 6   | KBD TX     |
| 7   | Power key (not connected, see Missing Features) |
| 8   | 5V         |

Pins 3 and 8 (5V) and pins 1 and 2 (GND) supply the Pico, the USB keyboard and
mouse, and the level shifters, as shown in the Power section above.
