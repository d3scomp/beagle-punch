# beagle-punch
Punch press simulator on BeagleBone Black.

The simulator consists of three elements:
1) Simulator on BBB - simulates a punch press at realtime
2) PC visualizer - connects to simulator on BBB via UART and displays the position of the punch press head and punches made
3) Skeleton of a controller for STM32F4 Discovery - connects to simulator on BBB via SPI and controls the operation of the punch press

Setup instructions can be found in instructions.txt
