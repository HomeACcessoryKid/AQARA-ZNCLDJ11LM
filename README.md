# AQARA-ZNCLDJ11LM
Aqara curtain motor ZNCLDJ11LM

This homekit software drives a curtain motor Aqara ZNCLDJ11LM as offered on
e.g. alibaba. It uses any ESP8266 with as little as 1MB flash. 
connect ESP Rx to motor TX and ESP GPIO-2 to motor RX and GND

We replace the ZigBee module with our own replacment module and it can run on the internal 3V3 powersupply ;-)

![](https://github.com/HomeACcessoryKid/HardWare/blob/master/Aqara-ZNCLDJ11LM/Aqara-5.png)
See [hardware repo](https://github.com/HomeACcessoryKid/HardWare/blob/master/Aqara-ZNCLDJ11LM/) for more images

From a homekit point of view, we use Eve to make three additional controls available:
- Calibrate(d)
- Reversed
- Firmware Update  
(the HOLD function does not actually work in the Eve app)

The motor has a AT-MEGA chip inside which tries to do a lot of smarts and we send a kind of MODBUS messages to control it.

If the motor is in uncalibrated mode, it will need to close and then open to kind out 0% and 100% the hard way (literally!)
The control can be set to off to then enable a new calibration cycle by setting it to on. 
A calibration cycle closes and opens from the ESP software until the motor reports being calibrated.
This is communicated by first showing the calibrate to off again until the motor reports ready.
If the curtain is reversed relative to 0% and 100%, then this control can set this straight. Toggeling it de-calibrates the motor.

If the power is lost on a calibrated motor, the program will close the curtain to find the 0% position again. No new calibration is needed.

While not being calibrated, the position is reported as 101%

If there is a block somewhere between 1 and 99%, the motor will report so and the homekit app will know it also.
![](https://github.com/HomeACcessoryKid/HardWare/blob/master/Aqara-ZNCLDJ11LM/Aqara-6.png)

For instructions on how to do the firmware update procedure, start at the [LCM repo](https://github.com/HomeACcessoryKid/life-cycle-manager/)  
Enjoy!  
PS. in the current version there are still some glitches after a power outage re-orientation, so pay attention for new releases which can fix this
