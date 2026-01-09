	insmod pwm_backlight.ko \
	backlight_dev_name="backlight_pwm0" default_brightness=0 max_brightness=101 \
	pwm_gpio=PC00 pwm_active_level=1 power_gpio=-1 \
	power_gpio_vaild_level=1 pwm_freq=10000 
