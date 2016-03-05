#include <linux/printk.h>	// pr_err, printk, etc
#include "fusb30x_global.h"	// Chip structure
#include "platform_helpers.h"	// Implementation details
#include "../core/platform.h"

/*******************************************************************************
* Function:        platform_set/get_vbus_lvl_enable
* Input:           VBUS_LVL - requested voltage
*                  Boolean - enable this voltage level
*                  Boolean - turn off other supported voltages
* Return:          Boolean - on or off
* Description:     Provide access to the VBUS control pins.
******************************************************************************/
void platform_set_vbus_lvl_enable(VBUS_LVL level, FSC_BOOL blnEnable,
				  FSC_BOOL blnDisableOthers)
{
#ifdef FPGA_BOARD
	FSC_U32 i;

	// Additional VBUS levels can be added here as needed.
	switch (level) {
	case VBUS_LVL_5V:
		// Enable/Disable the 5V Source
		fusb_GPIO_Set_VBus5v(blnEnable == TRUE ? true : false);
		break;
	case VBUS_LVL_12V:
		// Enable/Disable the 12V Source
		fusb_GPIO_Set_VBusOther(blnEnable == TRUE ? true : false);
		break;
	default:
		// Otherwise, do nothing.
		break;
	}

	// Turn off other levels, if requested
	if (blnDisableOthers
	    || ((level == VBUS_LVL_ALL) && (blnEnable == FALSE))) {
		i = 0;

		do {
			// Skip the current level
			if (i == level)
				continue;

			// Turn off the other level(s)
			platform_set_vbus_lvl_enable(i, FALSE, FALSE);
		} while (++i < VBUS_LVL_COUNT);
	}
#endif
	return;
}

FSC_BOOL platform_get_vbus_lvl_enable(VBUS_LVL level)
{
#ifdef FPGA_BOARD
	// Additional VBUS levels can be added here as needed.
	switch (level) {
	case VBUS_LVL_5V:
		// Return the state of the 5V VBUS Source.
		return fusb_GPIO_Get_VBus5v()? TRUE : FALSE;

	case VBUS_LVL_12V:
		// Return the state of the 12V VBUS Source.
		return fusb_GPIO_Get_VBusOther()? TRUE : FALSE;

	default:
		// Otherwise, return FALSE.
		return FALSE;
	}
#endif
	return TRUE;
}

/*******************************************************************************
* Function:        platform_set_vbus_discharge
* Input:           Boolean
* Return:          None
* Description:     Enable/Disable Vbus Discharge Path
******************************************************************************/
void platform_set_vbus_discharge(FSC_BOOL blnEnable)
{
	// TODO - Implement if required for platform
}

/*******************************************************************************
* Function:        platform_get_device_irq_state
* Input:           None
* Return:          Boolean.  TRUE = Interrupt Active
* Description:     Get the state of the INT_N pin.  INT_N is active low.  This
*                  function handles that by returning TRUE if the pin is
*                  pulled low indicating an active interrupt signal.
******************************************************************************/
FSC_BOOL platform_get_device_irq_state(void)
{
	return fusb_GPIO_Get_IntN()? TRUE : FALSE;
}

/*dump usbpd fifo log*/
#define regFIFO 0x43
void dump_usbpdlog(unsigned long RegisterAddress,
		   unsigned char DataLength,
		   unsigned char *Data, FSC_BOOL b_read)
{
	char log[256];
	char pd_read_mark[] = "[fusbpd]<<<<<";
	char pd_write_mark[] = "[fusbpd]>>>>>";
	int i, s_ret = 0;

	if (DataLength > ((sizeof(log) - sizeof(pd_read_mark)) / 3))
		return;
	if (regFIFO == RegisterAddress) {
		if (b_read)
			s_ret = snprintf(log, sizeof(log), "%s", pd_read_mark);
		else
			s_ret = snprintf(log, sizeof(log), "%s", pd_write_mark);
		for (i = 0; i < DataLength; i++)
			s_ret += snprintf(log + s_ret, sizeof(log) - s_ret,
					  " %2x", Data[i]);
		FUSB_LOG("%s", log);
	}
}

/*******************************************************************************
* Function:        platform_i2c_write
* Input:           SlaveAddress - Slave device bus address
*                  RegAddrLength - Register Address Byte Length
*                  DataLength - Length of data to transmit
*                  PacketSize - Maximum size of each transmitted packet
*                  IncSize - Number of bytes to send before incrementing addr
*                  RegisterAddress - Internal register address
*                  Data - Buffer of char data to transmit
* Return:          Error state
* Description:     Write a char buffer to the I2C peripheral.
******************************************************************************/
FSC_BOOL platform_i2c_write(FSC_U8 SlaveAddress,
			    FSC_U8 RegAddrLength,
			    FSC_U8 DataLength,
			    FSC_U8 PacketSize,
			    FSC_U8 IncSize,
			    FSC_U32 RegisterAddress, FSC_U8 * Data)
{
	FSC_BOOL ret = FALSE;
	if (Data == NULL) {
		pr_err("%s - Error: Write data buffer is NULL!\n", __func__);
		ret = FALSE;
	} else
	    if (fusb_I2C_WriteData((FSC_U8) RegisterAddress, DataLength, Data))
	{
		dump_usbpdlog(RegisterAddress, DataLength, Data, FALSE);
		ret = TRUE;
	} else			// I2C Write failure
	{
		ret = FALSE;	// Write data block to the device
	}
	return ret;
}

/*******************************************************************************
* Function:        platform_i2c_read
* Input:           SlaveAddress - Slave device bus address
*                  RegAddrLength - Register Address Byte Length
*                  DataLength - Length of data to attempt to read
*                  PacketSize - Maximum size of each received packet
*                  IncSize - Number of bytes to recv before incrementing addr
*                  RegisterAddress - Internal register address
*                  Data - Buffer for received char data
* Return:          Error state.
* Description:     Read char data from the I2C peripheral.
******************************************************************************/
FSC_BOOL platform_i2c_read(FSC_U8 SlaveAddress,
			   FSC_U8 RegAddrLength,
			   FSC_U8 DataLength,
			   FSC_U8 PacketSize,
			   FSC_U8 IncSize,
			   FSC_U32 RegisterAddress, FSC_U8 * Data)
{
	FSC_BOOL ret = FALSE;
	FSC_S32 i = 0;
	FSC_U8 temp = 0;
	struct fusb30x_chip *chip = fusb30x_GetChip();
	if (!chip) {
		pr_err("FUSB  %s - Error: Chip structure is NULL!\n", __func__);
		return FALSE;
	}

	if (Data == NULL) {
		pr_err("%s - Error: Read data buffer is NULL!\n", __func__);
		ret = FALSE;
	} else if (DataLength > 1 && chip->use_i2c_blocks)	// Do block reads if able and necessary
	{
		if (!fusb_I2C_ReadBlockData(RegisterAddress, DataLength, Data)) {
			ret = FALSE;
		} else {
			dump_usbpdlog(RegisterAddress, DataLength, Data, TRUE);
			ret = TRUE;
		}
	} else {
		for (i = 0; i < DataLength; i++) {
			if (fusb_I2C_ReadData
			    ((FSC_U8) RegisterAddress + i, &temp)) {
				Data[i] = temp;
				ret = TRUE;
			} else {
				ret = FALSE;
				break;
			}
		}
	}

	return ret;
}

/*****************************************************************************
* Function:        platform_enable_timer
* Input:           enable - TRUE to enable platform timer, FALSE to disable
* Return:          None
* Description:     Enables or disables platform timer
******************************************************************************/
void platform_enable_timer(FSC_BOOL enable)
{
	if (enable == TRUE) {
		fusb_StartTimers();
	} else {
		fusb_StopTimers();
	}
}

/*****************************************************************************
* Function:        platform_delay_10us
* Input:           delayCount - Number of 10us delays to wait
* Return:          None
* Description:     Perform a software delay in intervals of 10us.
******************************************************************************/
void platform_delay_10us(FSC_U32 delayCount)
{
	fusb_Delay10us(delayCount);
}
