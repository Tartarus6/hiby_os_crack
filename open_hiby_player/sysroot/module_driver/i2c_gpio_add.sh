insmod i2c_gpio_add.ko
cd /sys/module/i2c_gpio_add/parameters/
echo bus_num=3 rate=200000 scl=PB02 sda=PB03  > i2c_bus
