# AQARA-ZNCLDJ11LM
Aqara curtain motor homekit software. (c) 2018-2020 HomeAccessoryKid

This homekit software drives a curtain motor Aqara ZNCLDJ11LM as offered on
e.g. alibaba. It uses any ESP8266 with as little as 1MB flash. 
Connect ESP Rx to motor TX and ESP GPIO-2 to motor RX

We change the ZigBee module for our replacment module and it can run on the internal 3V3 powersupply ;-)
To prevent power surges dropping the voltage, apply a 10mF capacitor and a 1 ohm resistor (just to be sure). The capacitor allows fixing the module in the cap with some cheap glue so it can be removed if needed.

![](https://github.com/HomeACcessoryKid/HardWare/blob/master/Aqara-ZNCLDJ11LM/Aqara-5.png)
See [hardware repo](https://github.com/HomeACcessoryKid/HardWare/blob/master/Aqara-ZNCLDJ11LM/) for more images

From a homekit point of view, we use Eve to make three additional controls available:
- Calibrate(d)
- Reversed
- Firmware Update  
- (the HOLD function does not actually work in the Eve app and the Home app doesn't expose it all)

The motor has a AT-MEGA chip inside which tries to do a lot of smart things and it uses a kind of MODBUS messages to control it over 9600 baud serial 8N1.

If the motor is in uncalibrated mode, it will need to close and then open to find out 0% and 100% the hard way, literally!
The 'Calibrated' control can be set to off to then enable a new calibration cycle by setting it to on again. 
A calibration cycle closes and opens from the ESP software until the motor reports being calibrated.
This is communicated to the user by first setting the calibrate to off again until the motor reports ready.
If the motor direction is reversed relative to 0% and 100%, then the 'Reversed' control can be used.
Toggeling it de-calibrates the motor.

If the mains power is lost on a calibrated motor, the program will close the curtain to assert the 0% position again. No new calibration is needed.

While the system is not calibrated, the position is reported as 101%

If there is a blockage somewhere between 1 and 99%, the motor will report so and the homekit app will know how to report it. Moving to another position clear this message/state.
![](https://github.com/HomeACcessoryKid/AQARA-ZNCLDJ11LM/blob/master/images/Aqara-1.png)

For instructions on how to do the firmware update procedure, start at the [LCM repo](https://github.com/HomeACcessoryKid/life-cycle-manager/)  
Enjoy!  
PS. in the current version there are still some glitches after a power outage re-orientation, so pay attention for new releases which can fix this.  
PPS. for the moment there are debug messages emitted on UDP port 34567 on broadcast. Use netcat: nc -kulnw0 34567 to collect this output.  
