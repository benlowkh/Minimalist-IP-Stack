/*
 * physpeed.c
 *
 *  Created on: Apr 24, 2014
 *      Author: intern
 */

#include "adapter.h"
#include "sleep.h"

#define ADVERTISE_10HALF		0x0020  /* Try for 10mbps half-duplex  */
#define ADVERTISE_10FULL		0x0040  /* Try for 10mbps full-duplex  */
#define ADVERTISE_100HALF		0x0080  /* Try for 100mbps half-duplex */
#define ADVERTISE_100FULL		0x0100  /* Try for 100mbps full-duplex */

#define ADVERTISE_100_AND_10	(ADVERTISE_10FULL | ADVERTISE_100FULL | \
								ADVERTISE_10HALF | ADVERTISE_100HALF)
#define ADVERTISE_100			(ADVERTISE_100FULL | ADVERTISE_100HALF)
#define ADVERTISE_10			(ADVERTISE_10FULL | ADVERTISE_10HALF)

#define ADVERTISE_1000			0x0300


#define IEEE_CONTROL_REG_OFFSET				0
#define IEEE_STATUS_REG_OFFSET				1
#define IEEE_AUTONEGO_ADVERTISE_REG			4
#define IEEE_PARTNER_ABILITIES_1_REG_OFFSET	5
#define IEEE_1000_ADVERTISE_REG_OFFSET		9
#define IEEE_PARTNER_ABILITIES_3_REG_OFFSET	10
#define IEEE_COPPER_SPECIFIC_CONTROL_REG	16
#define IEEE_SPECIFIC_STATUS_REG			17
#define IEEE_COPPER_SPECIFIC_STATUS_REG_2	19
#define IEEE_CONTROL_REG_MAC				21
#define IEEE_PAGE_ADDRESS_REGISTER			22


#define IEEE_CTRL_1GBPS_LINKSPEED_MASK		0x2040
#define IEEE_CTRL_LINKSPEED_MASK			0x0040
#define IEEE_CTRL_LINKSPEED_1000M			0x0040
#define IEEE_CTRL_LINKSPEED_100M			0x2000
#define IEEE_CTRL_LINKSPEED_10M				0x0000
#define IEEE_CTRL_RESET_MASK				0x8000
#define IEEE_CTRL_AUTONEGOTIATE_ENABLE		0x1000
#define IEEE_STAT_AUTONEGOTIATE_CAPABLE		0x0008
#define IEEE_STAT_AUTONEGOTIATE_COMPLETE	0x0020
#define IEEE_STAT_AUTONEGOTIATE_RESTART		0x0200
#define IEEE_STAT_1GBPS_EXTENSIONS			0x0100
#define IEEE_AN1_ABILITY_MASK				0x1FE0
#define IEEE_AN3_ABILITY_MASK_1GBPS			0x0C00
#define IEEE_AN1_ABILITY_MASK_100MBPS		0x0380
#define IEEE_AN1_ABILITY_MASK_10MBPS		0x0060
#define IEEE_RGMII_TXRX_CLOCK_DELAYED_MASK	0x0030

#define IEEE_ASYMMETRIC_PAUSE_MASK			0x0800
#define IEEE_PAUSE_MASK						0x0400
#define IEEE_AUTONEG_ERROR_MASK				0x8000

#define PHY_DETECT_REG  1
#define PHY_DETECT_MASK 0x1808

/* Frequency setting */
#define SLCR_LOCK_ADDR			(XPS_SYS_CTRL_BASEADDR + 0x4)
#define SLCR_UNLOCK_ADDR		(XPS_SYS_CTRL_BASEADDR + 0x8)
#define SLCR_GEM0_CLK_CTRL_ADDR	(XPS_SYS_CTRL_BASEADDR + 0x140)
#define SLCR_GEM1_CLK_CTRL_ADDR	(XPS_SYS_CTRL_BASEADDR + 0x144)
#ifdef PEEP
#define SLCR_GEM_10M_CLK_CTRL_VALUE		0x00103031
#define SLCR_GEM_100M_CLK_CTRL_VALUE	0x00103001
#define SLCR_GEM_1G_CLK_CTRL_VALUE		0x00103011
#endif
#define SLCR_LOCK_KEY_VALUE 			0x767B
#define SLCR_UNLOCK_KEY_VALUE			0xDF0D
#define SLCR_ADDR_GEM_RST_CTRL			(XPS_SYS_CTRL_BASEADDR + 0x214)
#define EMACPS_SLCR_DIV_MASK			0xFC0FC0FF

#define EMAC0_BASE_ADDRESS				0xE000B000
#define EMAC1_BASE_ADDRESS				0xE000C000

static int detect_phy(XEmacPs *xemacpsp)
{
	u16 phy_reg;
	u32 phy_addr;

	for (phy_addr = 31; phy_addr > 0; phy_addr--) {
		XEmacPs_PhyRead(xemacpsp, phy_addr, PHY_DETECT_REG, &phy_reg);

		if ((phy_reg != 0xFFFF) && ((phy_reg & PHY_DETECT_MASK) == PHY_DETECT_MASK)) {
			return phy_addr;
		}
	}

        /* default to zero */
	return 0;
}

void SetUpSLCRDivisors(int mac_baseaddr, int speed)
{
	volatile u32 slcrBaseAddress;
#ifndef PEEP
	u32 SlcrDiv0;
	u32 SlcrDiv1;
	u32 SlcrTxClkCntrl;
#endif

	*(volatile unsigned int *)(SLCR_UNLOCK_ADDR) = SLCR_UNLOCK_KEY_VALUE;

	if (mac_baseaddr == EMAC0_BASE_ADDRESS) {
		slcrBaseAddress = SLCR_GEM0_CLK_CTRL_ADDR;
	} else {
		slcrBaseAddress = SLCR_GEM1_CLK_CTRL_ADDR;
	}
#ifdef PEEP
	if (speed == 1000) {
		*(volatile unsigned int *)(slcrBaseAddress) =
											SLCR_GEM_1G_CLK_CTRL_VALUE;
	} else if (speed == 100) {
		*(volatile unsigned int *)(slcrBaseAddress) =
											SLCR_GEM_100M_CLK_CTRL_VALUE;
	} else {
		*(volatile unsigned int *)(slcrBaseAddress) =
											SLCR_GEM_10M_CLK_CTRL_VALUE;
	}
#else
	if (speed == 1000) {
		if (mac_baseaddr == EMAC0_BASE_ADDRESS) {
#ifdef XPAR_PS7_ETHERNET_0_ENET_SLCR_1000MBPS_DIV0
			SlcrDiv0 = XPAR_PS7_ETHERNET_0_ENET_SLCR_1000MBPS_DIV0;
			SlcrDiv1 = XPAR_PS7_ETHERNET_0_ENET_SLCR_1000MBPS_DIV1;
#endif
		} else {
#ifdef XPAR_PS7_ETHERNET_1_ENET_SLCR_1000MBPS_DIV0
			SlcrDiv0 = XPAR_PS7_ETHERNET_1_ENET_SLCR_1000MBPS_DIV0;
			SlcrDiv1 = XPAR_PS7_ETHERNET_1_ENET_SLCR_1000MBPS_DIV1;
#endif
		}
	} else if (speed == 100) {
		if (mac_baseaddr == EMAC0_BASE_ADDRESS) {
#ifdef XPAR_PS7_ETHERNET_0_ENET_SLCR_100MBPS_DIV0
			SlcrDiv0 = XPAR_PS7_ETHERNET_0_ENET_SLCR_100MBPS_DIV0;
			SlcrDiv1 = XPAR_PS7_ETHERNET_0_ENET_SLCR_100MBPS_DIV1;
#endif
		} else {
#ifdef XPAR_PS7_ETHERNET_1_ENET_SLCR_100MBPS_DIV0
			SlcrDiv0 = XPAR_PS7_ETHERNET_1_ENET_SLCR_100MBPS_DIV0;
			SlcrDiv1 = XPAR_PS7_ETHERNET_1_ENET_SLCR_100MBPS_DIV1;
#endif
		}
	} else {
		if (mac_baseaddr == EMAC0_BASE_ADDRESS) {
#ifdef XPAR_PS7_ETHERNET_0_ENET_SLCR_10MBPS_DIV0
			SlcrDiv0 = XPAR_PS7_ETHERNET_0_ENET_SLCR_10MBPS_DIV0;
			SlcrDiv1 = XPAR_PS7_ETHERNET_0_ENET_SLCR_10MBPS_DIV1;
#endif
		} else {
#ifdef XPAR_PS7_ETHERNET_1_ENET_SLCR_10MBPS_DIV0
			SlcrDiv0 = XPAR_PS7_ETHERNET_1_ENET_SLCR_10MBPS_DIV0;
			SlcrDiv1 = XPAR_PS7_ETHERNET_1_ENET_SLCR_10MBPS_DIV1;
#endif
		}
	}
	SlcrTxClkCntrl = *(volatile unsigned int *)(slcrBaseAddress);
	SlcrTxClkCntrl &= EMACPS_SLCR_DIV_MASK;
	SlcrTxClkCntrl |= (SlcrDiv1 << 20);
	SlcrTxClkCntrl |= (SlcrDiv0 << 8);
	*(volatile unsigned int *)(slcrBaseAddress) = SlcrTxClkCntrl;
#endif
	*(volatile unsigned int *)(SLCR_LOCK_ADDR) = SLCR_LOCK_KEY_VALUE;
	return;
}

unsigned get_IEEE_phy_speed(XEmacPs *xemacpsp)
{
	u16 temp;
	u16 control;
	u16 status;
	u16 partner_capabilities;
	u32 phy_addr = detect_phy(xemacpsp);
	xil_printf("Start PHY autonegotiation \r\n");

	XEmacPs_PhyWrite(xemacpsp,phy_addr, IEEE_PAGE_ADDRESS_REGISTER, 2);
	XEmacPs_PhyRead(xemacpsp, phy_addr, IEEE_CONTROL_REG_MAC, &control);
	control |= IEEE_RGMII_TXRX_CLOCK_DELAYED_MASK;
	XEmacPs_PhyWrite(xemacpsp, phy_addr, IEEE_CONTROL_REG_MAC, control);

	XEmacPs_PhyWrite(xemacpsp, phy_addr, IEEE_PAGE_ADDRESS_REGISTER, 0);

	XEmacPs_PhyRead(xemacpsp, phy_addr, IEEE_AUTONEGO_ADVERTISE_REG, &control);
	control |= IEEE_ASYMMETRIC_PAUSE_MASK;
	control |= IEEE_PAUSE_MASK;
	control |= ADVERTISE_100;
	control |= ADVERTISE_10;
	XEmacPs_PhyWrite(xemacpsp, phy_addr, IEEE_AUTONEGO_ADVERTISE_REG, control);

	XEmacPs_PhyRead(xemacpsp, phy_addr, IEEE_1000_ADVERTISE_REG_OFFSET,
																	&control);
	control |= ADVERTISE_1000;
	XEmacPs_PhyWrite(xemacpsp, phy_addr, IEEE_1000_ADVERTISE_REG_OFFSET,
																	control);

	XEmacPs_PhyWrite(xemacpsp, phy_addr, IEEE_PAGE_ADDRESS_REGISTER, 0);
	XEmacPs_PhyRead(xemacpsp, phy_addr, IEEE_COPPER_SPECIFIC_CONTROL_REG,
																&control);
	control |= (7 << 12);	/* max number of gigabit attempts */
	control |= (1 << 11);	/* enable downshift */
	XEmacPs_PhyWrite(xemacpsp, phy_addr, IEEE_COPPER_SPECIFIC_CONTROL_REG,
																control);

	XEmacPs_PhyRead(xemacpsp, phy_addr, IEEE_CONTROL_REG_OFFSET, &control);
	control |= IEEE_CTRL_AUTONEGOTIATE_ENABLE;
	control |= IEEE_STAT_AUTONEGOTIATE_RESTART;
	XEmacPs_PhyWrite(xemacpsp, phy_addr, IEEE_CONTROL_REG_OFFSET, control);

	XEmacPs_PhyRead(xemacpsp, phy_addr, IEEE_CONTROL_REG_OFFSET, &control);
	control |= IEEE_CTRL_RESET_MASK;
	XEmacPs_PhyWrite(xemacpsp, phy_addr, IEEE_CONTROL_REG_OFFSET, control);

	while (1) {
		XEmacPs_PhyRead(xemacpsp, phy_addr, IEEE_CONTROL_REG_OFFSET, &control);
		if (control & IEEE_CTRL_RESET_MASK)
			continue;
		else
			break;

	}

	xil_printf("Waiting for PHY to complete autonegotiation.\r\n");

	XEmacPs_PhyRead(xemacpsp, phy_addr, IEEE_STATUS_REG_OFFSET, &status);
	while ( !(status & IEEE_STAT_AUTONEGOTIATE_COMPLETE) ) {
		sleep(1);
		XEmacPs_PhyRead(xemacpsp, phy_addr, IEEE_COPPER_SPECIFIC_STATUS_REG_2,
																	&temp);
		if (temp & IEEE_AUTONEG_ERROR_MASK) {
			xil_printf("Auto negotiation error \r\n");
		}
		XEmacPs_PhyRead(xemacpsp, phy_addr, IEEE_STATUS_REG_OFFSET,
																&status);
		}

	xil_printf("autonegotiation complete \r\n");

	XEmacPs_PhyRead(xemacpsp, phy_addr, IEEE_SPECIFIC_STATUS_REG,
										&partner_capabilities);
	if ( ((partner_capabilities >> 14) & 3) == 2)/* 1000Mbps */
		return 1000;
	else if ( ((partner_capabilities >> 14) & 3) == 1)/* 100Mbps */
		return 100;
	else					/* 10Mbps */
		return 10;
}

unsigned Phy_Setup (XEmacPs *xemacpsp){
	unsigned link_speed;
	link_speed = get_IEEE_phy_speed(xemacpsp);
	if (link_speed == 1000) {
		SetUpSLCRDivisors(xemacpsp->Config.BaseAddress,1000);
	} else if (link_speed == 100) {
		SetUpSLCRDivisors(xemacpsp->Config.BaseAddress,100);
	} else {
		SetUpSLCRDivisors(xemacpsp->Config.BaseAddress,10);
	}
	xil_printf("link speed: %d\r\n", link_speed);
	return link_speed;
}

