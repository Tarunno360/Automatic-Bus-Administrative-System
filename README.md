Automatic Bus Administrative System with Door Control 
Real Life Problem scenario: 
Our system can be integrated to prevent Bangladeshi buses from illegally taking passengers in the middle of the road and provide important administrative information about the bus to the admins. 
Features: 
1. USE CASE: Buses will have automatic door control which means the door will open when it arrives near a pre-defined station and safely close the door when it leaves the station area.
Equipments:Bluetooth module (HC-05)  to detect if the bus ( arduino connected with HC-05) is near a station ( Station will have another Arduino, connected with HC-05), IR sensors to safely close the door by detecting people and use servo motors to actually move the doors. 
2. The door of the bus will have multiple IR sensors which will count the number of passengers incoming and outgoing from the bus and this data will be stored in the system. There are two IR sensors, facing each other. If the first one detects the passenger first then the second one, it counts as the passenger going into the bus. If the second one detects the passenger first then the first one, it counts as the passenger going out of the bus
Equipments: Multiple IR sensors 
3. The system will also calculate the speed of the bus and show the speed in a lcd screen. 
Equipments: LCD display, magnet connected with the wheel of the bus and using hall effect to get the speed.
4. There will be an admin BUS station where the bus driver will transfer the bus data to the main station system. 
Equipments: Bluetooth module (HC-05) for data transfer and to detect if the bus has reached the admin station 
5. We will set a threshold of passengers at a time in the bus. If it crosses it then a buzzer will sound then the doors will get closed. 
Equipments: Buzzer to sound alarm, servo motor to close doors
6. Emergency Button for Passengers where a panic button is available inside the bus that passengers can press in case of harassment, fire, or medical emergency. Pressing it triggers an alarm and logs the event for review at the station. 
Equipments: Push button, another small buzzer for panic alarm 
7. The system monitors the internal bus temperature, especially during summer. If it crosses a comfortable threshold, the system starts the fan inside the bus via solar power. The solar system charges a battery, and then the battery is used to rotate the fan. 
Equipments: Simple solar panel, battery - solar panel charges the battery and fan is turned on/off by battery 
8. Driver Authentication with ID Tag or PIN Only authorized drivers can activate the bus system. The driver must scan an ID card (via RFID) on a RFID module before he can manually override the door control using the button. 
Equipments: RFID reader module and tag
