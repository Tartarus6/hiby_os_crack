insmod keyboard_gpio_add.ko
cd /sys/module/keyboard_gpio_add/parameters/ 
echo alloc=16 > keyboard
echo gpio="PC31" key_code=116 tag="KEY_POWER" active_level=0 wakeup=y  > keyboard
echo gpio="PC28" key_code=165 tag="KEY_PREVIOUSSONG" active_level=1 wakeup=y  > keyboard
echo register > keyboard
