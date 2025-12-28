insmod soc_msc.ko \
wifi_power_on=-1 \
wifi_power_on_level=0 \
wifi_reg_on=-1 \
wifi_reg_on_level=0 \
	msc0_is_enable=1 \
		msc0_voltage_min=1800 \
		msc0_voltage_max=3300 \
		msc0_rm_method=manual \
		msc0_cd_method=non-removable \
		msc0_bus_width=4 \
		msc0_speed=sd_card \
		msc0_max_frequency=50000000 \
		msc0_cap_power_off_card=0 \
		msc0_cap_mmc_hw_reset=0 \
		msc0_cap_sdio_irq=0 \
		msc0_full_pwr_cycle=0 \
		msc0_keep_power_in_suspend=y \
		msc0_enable_sdio_wakeup=0 \
		msc0_dsr=0x404 \
		msc0_pio_mode=0 \
		msc0_enable_autocmd12=0 \
		msc0_enable_cpm_rx_tuning=0 \
		msc0_enable_cpm_tx_tuning=0 \
		msc0_sdio_clk=0 \
		msc0_rst=-1 msc0_rst_enable_level=-1 \
		msc0_wp=-1 msc0_wp_enable_level=-1 \
		msc0_pwr=-1 msc0_pwr_enable_level=-1 \
msc0_pwr_regulator=-1 \
		msc0_cd=-1 msc0_cd_enable_level=-1 \
		msc0_sdr=-1 msc0_sdr_enable_level=-1 \
	msc1_is_enable=1 \
		msc1_voltage_min=1800 \
		msc1_voltage_max=3300 \
		msc1_rm_method=removable \
		msc1_cd_method=cd-inverted \
		msc1_bus_width=4 \
		msc1_speed=sd_card \
		msc1_max_frequency=50000000 \
		msc1_cap_power_off_card=y \
		msc1_cap_mmc_hw_reset=0 \
		msc1_cap_sdio_irq=0 \
		msc1_full_pwr_cycle=0 \
		msc1_keep_power_in_suspend=0 \
		msc1_enable_sdio_wakeup=0 \
		msc1_dsr=0x404 \
		msc1_pio_mode=0 \
		msc1_enable_autocmd12=0 \
		msc1_enable_cpm_rx_tuning=0 \
		msc1_enable_cpm_tx_tuning=0 \
		msc1_sdio_clk=0 \
		msc1_rst=-1 msc1_rst_enable_level=-1 \
		msc1_wp=-1 msc1_wp_enable_level=-1 \
		msc1_pwr=PF14 msc1_pwr_enable_level=1 \
msc1_pwr_regulator=-1 \
		msc1_cd=PB22 msc1_cd_enable_level=0 \
		msc1_sdr=-1 msc1_sdr_enable_level=-1 \
