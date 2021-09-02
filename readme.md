#Fan control
Control external fan by switching GPIO 12 defined as POWER_PIN

##Dependency
Libmraa - https://wiki.radxa.com/Rockpi4/dev/libmraa

##Compilation
g++ -o fancontrol fancontrol.cpp -lmraa

##Temperature
CPU temperature is read from '/sys/class/thermal/thermal_zone0/temp' file

