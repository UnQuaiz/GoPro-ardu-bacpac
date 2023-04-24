# Herobus pinout of GoPro Hero 3+ Black

There is a receptacle on the back of newer GoPro cameras.

The model number of the receptacle is `JAE DD1R030HA1R1300` or `JAE DD1R030JA7R1300`.
The receptacle mates male connectors `DD1B030HA1R500`, `DD1B030VA1`, and `DD1P030MA1`. [cf. JAE documentations](http://www.jae-connector.com/en/general_en.cfm?series_code=DD1)

The connectors look very similar ones that are used by Apple’s iPods or older iPhones, but they have different keys and can’t pair.

MewPro uses a compatible connector to `DD1P030MA1` and, for some technical reasons, the footprint on the PCB is designed for non-genuine one. 

You can buy this non-genuine but compatible connector from [Ridax](http://www.chargeconverter.com/store/index.php?route=product/product&product_id=150) store where the product name is `“DOCK CONNECTOR FOR PCB HOLE MOUNTING FOR SANDISK/GOPRO”`, or you can order us a variation of MewPro that already includes a Herobus connector soldered.

_Note: Pin numbering is in the reverse order to, for example, [Ridax’s breakout PCB](http://www.chargeconverter.com/store/index.php?route=product/product&product_id=151) or some GoPro forum posts. According to JAE’s documents they are wrong and the reverse order to them IS correct. Thus from now on our site and our products shall adopt the JAE’s pin order._

## Herobus pinout of GoPro Hero 3+ Black

pin | I/O | label | description | remark
:--- | :--- | :--- | :--- | :---
1|-|GND|ground|
2|I/O|I2CSCL|I²C clock|(pulled up by camera)
3|I/O|I2CSDA|I²C data|(pulled up by camera)
4|-|GND|ground|
5|I|BATTIN|battery input|
6|I|BATTIN|battery input|
7|O|3V8OUT|unregulated power out 3.8V|0V when camera is off.
8|O|3V8STBY|unregulated power out 3.8V|always keep 3.8V unless battery is out.
9|I|BPRDY|bacpac ready|set low I²C EEPROM to be read by camera. (pulled up by camera)
10|I|VSYNC|3D genlock vertical sync|[LCD touch bacpac only: pulled up. set low to sleep LCD touch bacpac]
11|-|PIN11||[LCD touch bacpac only: low on audio ready]
12|-|PIN12||[LCD touch bacpac only: high on video ready]
13|-|PIN13||
14|I|TRIG|3D sync trigger|
15|I|I2CINT|I²C interrupt|read request from slave. (pulled up by camera)
16|I|AUDIOIN_L|left mic in|impedance 2.2k, reserved power bias 3.3V, unbalanced
17|I|AUDIOIN_R|right mic in|impedance 2.2k, reserved power bias 3.3V, unbalanced
18|-|PIN18|[LCD touch bacpac only: playback button]|(pulled up by camera)
19|I|PWRBTN|power/mode button|(pulled up by camera)
20|I|HSYNC|3D genlock horizontal sync|[LCD touch bacpac only: left audio out]
21|-|PIN21||[LCD touch bacpac only: right audio out]
22|-|GND|ground|
23|I/O|USB_D−|USB D−|
24|I/O|USB_D+|USB D+|
25|-|USB_VBUS|USB VBUS|charging camera battery through this pin is not possible.
26|-|USB_VBUS|USB VBUS|charging camera battery through this pin is not possible.
27|-|PIN27||
28|O|HBUSRDY|high on Herobus ready|
29|O|VIDEO|composite video out|
30|-|GND|ground|
