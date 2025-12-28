#Reset
hcitool cmd 03 03
# Write_BD_ADDR
# hcitool cmd 3f 01 66 55 44 33 22 11
# Read_BD_ADDR
# hcitool cmd 04 09
# Write_Scan_Enable
hcitool cmd 03 1a 03
# Set_Event_Filter
hcitool cmd 03 05 02 00 03
# Enable_Device_Under_Test_Mode
hcitool cmd 06 03