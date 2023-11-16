# TI335x MII
Linux TI335x: MII Connects to Marvell Switch 88E6097F

TI3352 connects to Marvell switch controller 88E6097F via MII. Switch controller port 9 acts as a PHY and connects to TI3352 MAC 0. Linux kernel is 4.19. User application uses Marvell DSDT tool to control switch registers.

Hardware:

Pins to configure with resistors:
1. SW_Mode[1:0] pins: 00 CPU mode: all ports disabled. 
10 Test mode: all ports forwarding.

2. Px_MODE[2:0] pins: 010 for Px_GTXCLK = 25 MHz, 100Base

Software:

Change Linux device tree to use fixed-link PHY driver, not davinci_mdio driver. No change for u-boot.

&cpsw_emac0 {
	//phy_id = <&davinci_mdio>, <0>;
	fixed-link = <1 1 100 0 0>;
	phy-mode = "mii";
};

&mac {
 slaves = <1>;
 pinctrl-names = "default", "sleep";
 pinctrl-0 = <&cpsw_default>;
 pinctrl-1 = <&cpsw_sleep>;
 status = "okay";
};

To use Marvell DSDT, you must be able to read and write TI3352 MDIO registers. Here is the method I use.

a). Access TI335x MDIO controller from Linux userspace with “mmap” command. In AM3352 the MDIO controller address is 0x4a101000, map this address and the length is 0x90 bytes.

b). Enable MDIO control in the address 0x4a101004 ( MDIOCONTROL register) with data 0x410000ff.

c). Read and write switch controller registers with address 0x4a101080 ( MDIOUSERACCESS0 register).

d). 88E6097F port registers: PHY address from 16 (0x10, port0) to 26 (0x1a, port 10), register number from 0 to 31. 

Read/Write address 25 for port 9 registers:
Read Register 0: last 3 bits show Px_MODE[2:0] settings. Should be 010. 
Write 0x003D to Register 1: enable communication between TI and switch controller P9. --Very Importmant.

