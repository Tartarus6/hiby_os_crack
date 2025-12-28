insmod leds_sgm31324_add.ko
cd /sys/module/leds_sgm31324_add/parameters/
echo i2c_bus_num=0 > sgm31324
echo enable_gpio=-1 > sgm31324
echo enable_active_level=-1 > sgm31324
echo wled_gpio=PF11 > sgm31324
echo wled_active_level=1 > sgm31324
echo wled_pattern_id=11 > sgm31324
echo alloc=16 > sgm31324
echo regs="00000000540001010706"  > sgm31324
echo regs="000000004500380B0000"  > sgm31324
echo regs="00000000540000060600"  > sgm31324
echo regs="00000000450040040000"  > sgm31324
echo regs="00000000000000000006"  > sgm31324
echo regs="001E7D0042BB2F000006"  > sgm31324
echo regs="000E7D0042663F000006"  > sgm31324
echo regs="00000000440000050006"  > sgm31324
echo regs="00000000540000000D06"  > sgm31324
echo regs="0000000051001C000506"  > sgm31324
echo regs="00000000000000000006"  > sgm31324
echo register > sgm31324
