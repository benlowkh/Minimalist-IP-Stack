/*
 * platform.c
 *
 *  Created on: Apr 25, 2014
 *      Author: intern
 */

#ifdef __arm__

#include "platform.h"
#include "xil_cache.h"
#include "xscugic.h"

#define EMACPS_DEVICE_ID	XPAR_XEMACPS_0_DEVICE_ID
#define INTC_DEVICE_ID		XPAR_SCUGIC_SINGLE_DEVICE_ID

void platform_setup_interrupts(void)
{
	Xil_ExceptionInit();

	XScuGic_DeviceInitialize(INTC_DEVICE_ID);

	/*
	 * Connect the interrupt controller interrupt handler to the hardware
	 * interrupt handling logic in the processor.
	 */
	Xil_ExceptionRegisterHandler(XIL_EXCEPTION_ID_IRQ_INT,
			(Xil_ExceptionHandler)XScuGic_DeviceInterruptHandler,
			(void *)INTC_DEVICE_ID);

	return;
}

void platform_enable_interrupts()
{
	/*
	 * Enable non-critical exceptions.
	 */
	Xil_ExceptionEnable();
	return;
}

void init_platform()
{
	platform_setup_interrupts();

	return;
}

void cleanup_platform()
{
	Xil_ICacheDisable();
	Xil_DCacheDisable();
	return;
}
#endif

