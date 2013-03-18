/*******************************************************************************
 *
 * Module Name: rsdumpinfo - Tables used to display resource descriptors.
 *
 ******************************************************************************/

/*
 * Copyright (C) 2000 - 2013, Intel Corp.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions, and the following disclaimer,
 *    without modification.
 * 2. Redistributions in binary form must reproduce at minimum a disclaimer
 *    substantially similar to the "NO WARRANTY" disclaimer below
 *    ("Disclaimer") and any redistribution must be conditioned upon
 *    including a substantially similar Disclaimer requirement for further
 *    binary redistribution.
 * 3. Neither the names of the above-listed copyright holders nor the names
 *    of any contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * Alternatively, this software may be distributed under the terms of the
 * GNU General Public License ("GPL") version 2 as published by the Free
 * Software Foundation.
 *
 * NO WARRANTY
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDERS OR CONTRIBUTORS BE LIABLE FOR SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGES.
 */

#include <acpi/acpi.h>
#include "accommon.h"
#include "acresrc.h"

#define _COMPONENT          ACPI_RESOURCES
ACPI_MODULE_NAME("rsdumpinfo")

#if defined(ACPI_DEBUG_OUTPUT) || defined(ACPI_DEBUGGER)
#define ACPI_RSD_OFFSET(f)          (u8) ACPI_OFFSET (union acpi_resource_data,f)
#define ACPI_PRT_OFFSET(f)          (u8) ACPI_OFFSET (struct acpi_pci_routing_table,f)
#define ACPI_RSD_TABLE_SIZE(name)   (sizeof(name) / sizeof (struct acpi_rsdump_info))
/*******************************************************************************
 *
 * Resource Descriptor info tables
 *
 * Note: The first table entry must be a Title or Literal and must contain
 * the table length (number of table entries)
 *
 ******************************************************************************/
struct acpi_rsdump_info acpi_rs_dump_irq[7] = {
	{ACPI_RSD_TITLE, ACPI_RSD_TABLE_SIZE(acpi_rs_dump_irq), "IRQ", NULL},
	{ACPI_RSD_UINT8, ACPI_RSD_OFFSET(irq.descriptor_length),
	 "Descriptor Length", NULL},
	{ACPI_RSD_1BITFLAG, ACPI_RSD_OFFSET(irq.triggering), "Triggering",
	 acpi_gbl_he_decode},
	{ACPI_RSD_1BITFLAG, ACPI_RSD_OFFSET(irq.polarity), "Polarity",
	 acpi_gbl_ll_decode},
	{ACPI_RSD_2BITFLAG, ACPI_RSD_OFFSET(irq.sharable), "Sharing",
	 acpi_gbl_shr_decode},
	{ACPI_RSD_UINT8, ACPI_RSD_OFFSET(irq.interrupt_count),
	 "Interrupt Count", NULL},
	{ACPI_RSD_SHORTLIST, ACPI_RSD_OFFSET(irq.interrupts[0]),
	 "Interrupt List", NULL}
};

struct acpi_rsdump_info acpi_rs_dump_dma[6] = {
	{ACPI_RSD_TITLE, ACPI_RSD_TABLE_SIZE(acpi_rs_dump_dma), "DMA", NULL},
	{ACPI_RSD_2BITFLAG, ACPI_RSD_OFFSET(dma.type), "Speed",
	 acpi_gbl_typ_decode},
	{ACPI_RSD_1BITFLAG, ACPI_RSD_OFFSET(dma.bus_master), "Mastering",
	 acpi_gbl_bm_decode},
	{ACPI_RSD_2BITFLAG, ACPI_RSD_OFFSET(dma.transfer), "Transfer Type",
	 acpi_gbl_siz_decode},
	{ACPI_RSD_UINT8, ACPI_RSD_OFFSET(dma.channel_count), "Channel Count",
	 NULL},
	{ACPI_RSD_SHORTLIST, ACPI_RSD_OFFSET(dma.channels[0]), "Channel List",
	 NULL}
};

struct acpi_rsdump_info acpi_rs_dump_start_dpf[4] = {
	{ACPI_RSD_TITLE, ACPI_RSD_TABLE_SIZE(acpi_rs_dump_start_dpf),
	 "Start-Dependent-Functions", NULL},
	{ACPI_RSD_UINT8, ACPI_RSD_OFFSET(start_dpf.descriptor_length),
	 "Descriptor Length", NULL},
	{ACPI_RSD_2BITFLAG, ACPI_RSD_OFFSET(start_dpf.compatibility_priority),
	 "Compatibility Priority", acpi_gbl_config_decode},
	{ACPI_RSD_2BITFLAG, ACPI_RSD_OFFSET(start_dpf.performance_robustness),
	 "Performance/Robustness", acpi_gbl_config_decode}
};

struct acpi_rsdump_info acpi_rs_dump_end_dpf[1] = {
	{ACPI_RSD_TITLE, ACPI_RSD_TABLE_SIZE(acpi_rs_dump_end_dpf),
	 "End-Dependent-Functions", NULL}
};

struct acpi_rsdump_info acpi_rs_dump_io[6] = {
	{ACPI_RSD_TITLE, ACPI_RSD_TABLE_SIZE(acpi_rs_dump_io), "I/O", NULL},
	{ACPI_RSD_1BITFLAG, ACPI_RSD_OFFSET(io.io_decode), "Address Decoding",
	 acpi_gbl_io_decode},
	{ACPI_RSD_UINT16, ACPI_RSD_OFFSET(io.minimum), "Address Minimum", NULL},
	{ACPI_RSD_UINT16, ACPI_RSD_OFFSET(io.maximum), "Address Maximum", NULL},
	{ACPI_RSD_UINT8, ACPI_RSD_OFFSET(io.alignment), "Alignment", NULL},
	{ACPI_RSD_UINT8, ACPI_RSD_OFFSET(io.address_length), "Address Length",
	 NULL}
};

struct acpi_rsdump_info acpi_rs_dump_fixed_io[3] = {
	{ACPI_RSD_TITLE, ACPI_RSD_TABLE_SIZE(acpi_rs_dump_fixed_io),
	 "Fixed I/O", NULL},
	{ACPI_RSD_UINT16, ACPI_RSD_OFFSET(fixed_io.address), "Address", NULL},
	{ACPI_RSD_UINT8, ACPI_RSD_OFFSET(fixed_io.address_length),
	 "Address Length", NULL}
};

struct acpi_rsdump_info acpi_rs_dump_vendor[3] = {
	{ACPI_RSD_TITLE, ACPI_RSD_TABLE_SIZE(acpi_rs_dump_vendor),
	 "Vendor Specific", NULL},
	{ACPI_RSD_UINT16, ACPI_RSD_OFFSET(vendor.byte_length), "Length", NULL},
	{ACPI_RSD_LONGLIST, ACPI_RSD_OFFSET(vendor.byte_data[0]), "Vendor Data",
	 NULL}
};

struct acpi_rsdump_info acpi_rs_dump_end_tag[1] = {
	{ACPI_RSD_TITLE, ACPI_RSD_TABLE_SIZE(acpi_rs_dump_end_tag), "EndTag",
	 NULL}
};

struct acpi_rsdump_info acpi_rs_dump_memory24[6] = {
	{ACPI_RSD_TITLE, ACPI_RSD_TABLE_SIZE(acpi_rs_dump_memory24),
	 "24-Bit Memory Range", NULL},
	{ACPI_RSD_1BITFLAG, ACPI_RSD_OFFSET(memory24.write_protect),
	 "Write Protect", acpi_gbl_rw_decode},
	{ACPI_RSD_UINT16, ACPI_RSD_OFFSET(memory24.minimum), "Address Minimum",
	 NULL},
	{ACPI_RSD_UINT16, ACPI_RSD_OFFSET(memory24.maximum), "Address Maximum",
	 NULL},
	{ACPI_RSD_UINT16, ACPI_RSD_OFFSET(memory24.alignment), "Alignment",
	 NULL},
	{ACPI_RSD_UINT16, ACPI_RSD_OFFSET(memory24.address_length),
	 "Address Length", NULL}
};

struct acpi_rsdump_info acpi_rs_dump_memory32[6] = {
	{ACPI_RSD_TITLE, ACPI_RSD_TABLE_SIZE(acpi_rs_dump_memory32),
	 "32-Bit Memory Range", NULL},
	{ACPI_RSD_1BITFLAG, ACPI_RSD_OFFSET(memory32.write_protect),
	 "Write Protect", acpi_gbl_rw_decode},
	{ACPI_RSD_UINT32, ACPI_RSD_OFFSET(memory32.minimum), "Address Minimum",
	 NULL},
	{ACPI_RSD_UINT32, ACPI_RSD_OFFSET(memory32.maximum), "Address Maximum",
	 NULL},
	{ACPI_RSD_UINT32, ACPI_RSD_OFFSET(memory32.alignment), "Alignment",
	 NULL},
	{ACPI_RSD_UINT32, ACPI_RSD_OFFSET(memory32.address_length),
	 "Address Length", NULL}
};

struct acpi_rsdump_info acpi_rs_dump_fixed_memory32[4] = {
	{ACPI_RSD_TITLE, ACPI_RSD_TABLE_SIZE(acpi_rs_dump_fixed_memory32),
	 "32-Bit Fixed Memory Range", NULL},
	{ACPI_RSD_1BITFLAG, ACPI_RSD_OFFSET(fixed_memory32.write_protect),
	 "Write Protect", acpi_gbl_rw_decode},
	{ACPI_RSD_UINT32, ACPI_RSD_OFFSET(fixed_memory32.address), "Address",
	 NULL},
	{ACPI_RSD_UINT32, ACPI_RSD_OFFSET(fixed_memory32.address_length),
	 "Address Length", NULL}
};

struct acpi_rsdump_info acpi_rs_dump_address16[8] = {
	{ACPI_RSD_TITLE, ACPI_RSD_TABLE_SIZE(acpi_rs_dump_address16),
	 "16-Bit WORD Address Space", NULL},
	{ACPI_RSD_ADDRESS, 0, NULL, NULL},
	{ACPI_RSD_UINT16, ACPI_RSD_OFFSET(address16.granularity), "Granularity",
	 NULL},
	{ACPI_RSD_UINT16, ACPI_RSD_OFFSET(address16.minimum), "Address Minimum",
	 NULL},
	{ACPI_RSD_UINT16, ACPI_RSD_OFFSET(address16.maximum), "Address Maximum",
	 NULL},
	{ACPI_RSD_UINT16, ACPI_RSD_OFFSET(address16.translation_offset),
	 "Translation Offset", NULL},
	{ACPI_RSD_UINT16, ACPI_RSD_OFFSET(address16.address_length),
	 "Address Length", NULL},
	{ACPI_RSD_SOURCE, ACPI_RSD_OFFSET(address16.resource_source), NULL, NULL}
};

struct acpi_rsdump_info acpi_rs_dump_address32[8] = {
	{ACPI_RSD_TITLE, ACPI_RSD_TABLE_SIZE(acpi_rs_dump_address32),
	 "32-Bit DWORD Address Space", NULL},
	{ACPI_RSD_ADDRESS, 0, NULL, NULL},
	{ACPI_RSD_UINT32, ACPI_RSD_OFFSET(address32.granularity), "Granularity",
	 NULL},
	{ACPI_RSD_UINT32, ACPI_RSD_OFFSET(address32.minimum), "Address Minimum",
	 NULL},
	{ACPI_RSD_UINT32, ACPI_RSD_OFFSET(address32.maximum), "Address Maximum",
	 NULL},
	{ACPI_RSD_UINT32, ACPI_RSD_OFFSET(address32.translation_offset),
	 "Translation Offset", NULL},
	{ACPI_RSD_UINT32, ACPI_RSD_OFFSET(address32.address_length),
	 "Address Length", NULL},
	{ACPI_RSD_SOURCE, ACPI_RSD_OFFSET(address32.resource_source), NULL, NULL}
};

struct acpi_rsdump_info acpi_rs_dump_address64[8] = {
	{ACPI_RSD_TITLE, ACPI_RSD_TABLE_SIZE(acpi_rs_dump_address64),
	 "64-Bit QWORD Address Space", NULL},
	{ACPI_RSD_ADDRESS, 0, NULL, NULL},
	{ACPI_RSD_UINT64, ACPI_RSD_OFFSET(address64.granularity), "Granularity",
	 NULL},
	{ACPI_RSD_UINT64, ACPI_RSD_OFFSET(address64.minimum), "Address Minimum",
	 NULL},
	{ACPI_RSD_UINT64, ACPI_RSD_OFFSET(address64.maximum), "Address Maximum",
	 NULL},
	{ACPI_RSD_UINT64, ACPI_RSD_OFFSET(address64.translation_offset),
	 "Translation Offset", NULL},
	{ACPI_RSD_UINT64, ACPI_RSD_OFFSET(address64.address_length),
	 "Address Length", NULL},
	{ACPI_RSD_SOURCE, ACPI_RSD_OFFSET(address64.resource_source), NULL, NULL}
};

struct acpi_rsdump_info acpi_rs_dump_ext_address64[8] = {
	{ACPI_RSD_TITLE, ACPI_RSD_TABLE_SIZE(acpi_rs_dump_ext_address64),
	 "64-Bit Extended Address Space", NULL},
	{ACPI_RSD_ADDRESS, 0, NULL, NULL},
	{ACPI_RSD_UINT64, ACPI_RSD_OFFSET(ext_address64.granularity),
	 "Granularity", NULL},
	{ACPI_RSD_UINT64, ACPI_RSD_OFFSET(ext_address64.minimum),
	 "Address Minimum", NULL},
	{ACPI_RSD_UINT64, ACPI_RSD_OFFSET(ext_address64.maximum),
	 "Address Maximum", NULL},
	{ACPI_RSD_UINT64, ACPI_RSD_OFFSET(ext_address64.translation_offset),
	 "Translation Offset", NULL},
	{ACPI_RSD_UINT64, ACPI_RSD_OFFSET(ext_address64.address_length),
	 "Address Length", NULL},
	{ACPI_RSD_UINT64, ACPI_RSD_OFFSET(ext_address64.type_specific),
	 "Type-Specific Attribute", NULL}
};

struct acpi_rsdump_info acpi_rs_dump_ext_irq[8] = {
	{ACPI_RSD_TITLE, ACPI_RSD_TABLE_SIZE(acpi_rs_dump_ext_irq),
	 "Extended IRQ", NULL},
	{ACPI_RSD_1BITFLAG, ACPI_RSD_OFFSET(extended_irq.producer_consumer),
	 "Type", acpi_gbl_consume_decode},
	{ACPI_RSD_1BITFLAG, ACPI_RSD_OFFSET(extended_irq.triggering),
	 "Triggering", acpi_gbl_he_decode},
	{ACPI_RSD_1BITFLAG, ACPI_RSD_OFFSET(extended_irq.polarity), "Polarity",
	 acpi_gbl_ll_decode},
	{ACPI_RSD_2BITFLAG, ACPI_RSD_OFFSET(extended_irq.sharable), "Sharing",
	 acpi_gbl_shr_decode},
	{ACPI_RSD_SOURCE, ACPI_RSD_OFFSET(extended_irq.resource_source), NULL,
	 NULL},
	{ACPI_RSD_UINT8, ACPI_RSD_OFFSET(extended_irq.interrupt_count),
	 "Interrupt Count", NULL},
	{ACPI_RSD_DWORDLIST, ACPI_RSD_OFFSET(extended_irq.interrupts[0]),
	 "Interrupt List", NULL}
};

struct acpi_rsdump_info acpi_rs_dump_generic_reg[6] = {
	{ACPI_RSD_TITLE, ACPI_RSD_TABLE_SIZE(acpi_rs_dump_generic_reg),
	 "Generic Register", NULL},
	{ACPI_RSD_UINT8, ACPI_RSD_OFFSET(generic_reg.space_id), "Space ID",
	 NULL},
	{ACPI_RSD_UINT8, ACPI_RSD_OFFSET(generic_reg.bit_width), "Bit Width",
	 NULL},
	{ACPI_RSD_UINT8, ACPI_RSD_OFFSET(generic_reg.bit_offset), "Bit Offset",
	 NULL},
	{ACPI_RSD_UINT8, ACPI_RSD_OFFSET(generic_reg.access_size),
	 "Access Size", NULL},
	{ACPI_RSD_UINT64, ACPI_RSD_OFFSET(generic_reg.address), "Address", NULL}
};

struct acpi_rsdump_info acpi_rs_dump_gpio[16] = {
	{ACPI_RSD_TITLE, ACPI_RSD_TABLE_SIZE(acpi_rs_dump_gpio), "GPIO", NULL},
	{ACPI_RSD_UINT8, ACPI_RSD_OFFSET(gpio.revision_id), "RevisionId", NULL},
	{ACPI_RSD_UINT8, ACPI_RSD_OFFSET(gpio.connection_type),
	 "ConnectionType", acpi_gbl_ct_decode},
	{ACPI_RSD_1BITFLAG, ACPI_RSD_OFFSET(gpio.producer_consumer),
	 "ProducerConsumer", acpi_gbl_consume_decode},
	{ACPI_RSD_UINT8, ACPI_RSD_OFFSET(gpio.pin_config), "PinConfig",
	 acpi_gbl_ppc_decode},
	{ACPI_RSD_2BITFLAG, ACPI_RSD_OFFSET(gpio.sharable), "Sharing",
	 acpi_gbl_shr_decode},
	{ACPI_RSD_2BITFLAG, ACPI_RSD_OFFSET(gpio.io_restriction),
	 "IoRestriction", acpi_gbl_ior_decode},
	{ACPI_RSD_1BITFLAG, ACPI_RSD_OFFSET(gpio.triggering), "Triggering",
	 acpi_gbl_he_decode},
	{ACPI_RSD_2BITFLAG, ACPI_RSD_OFFSET(gpio.polarity), "Polarity",
	 acpi_gbl_ll_decode},
	{ACPI_RSD_UINT16, ACPI_RSD_OFFSET(gpio.drive_strength), "DriveStrength",
	 NULL},
	{ACPI_RSD_UINT16, ACPI_RSD_OFFSET(gpio.debounce_timeout),
	 "DebounceTimeout", NULL},
	{ACPI_RSD_SOURCE, ACPI_RSD_OFFSET(gpio.resource_source),
	 "ResourceSource", NULL},
	{ACPI_RSD_UINT16, ACPI_RSD_OFFSET(gpio.pin_table_length),
	 "PinTableLength", NULL},
	{ACPI_RSD_WORDLIST, ACPI_RSD_OFFSET(gpio.pin_table), "PinTable", NULL},
	{ACPI_RSD_UINT16, ACPI_RSD_OFFSET(gpio.vendor_length), "VendorLength",
	 NULL},
	{ACPI_RSD_SHORTLISTX, ACPI_RSD_OFFSET(gpio.vendor_data), "VendorData",
	 NULL},
};

struct acpi_rsdump_info acpi_rs_dump_fixed_dma[4] = {
	{ACPI_RSD_TITLE, ACPI_RSD_TABLE_SIZE(acpi_rs_dump_fixed_dma),
	 "FixedDma", NULL},
	{ACPI_RSD_UINT16, ACPI_RSD_OFFSET(fixed_dma.request_lines),
	 "RequestLines", NULL},
	{ACPI_RSD_UINT16, ACPI_RSD_OFFSET(fixed_dma.channels), "Channels",
	 NULL},
	{ACPI_RSD_UINT8, ACPI_RSD_OFFSET(fixed_dma.width), "TransferWidth",
	 acpi_gbl_dts_decode},
};

#define ACPI_RS_DUMP_COMMON_SERIAL_BUS \
	{ACPI_RSD_UINT8,    ACPI_RSD_OFFSET (common_serial_bus.revision_id),    "RevisionId",               NULL}, \
	{ACPI_RSD_UINT8,    ACPI_RSD_OFFSET (common_serial_bus.type),           "Type",                     acpi_gbl_sbt_decode}, \
	{ACPI_RSD_1BITFLAG, ACPI_RSD_OFFSET (common_serial_bus.producer_consumer), "ProducerConsumer",      acpi_gbl_consume_decode}, \
	{ACPI_RSD_1BITFLAG, ACPI_RSD_OFFSET (common_serial_bus.slave_mode),     "SlaveMode",                acpi_gbl_sm_decode}, \
	{ACPI_RSD_UINT8,    ACPI_RSD_OFFSET (common_serial_bus.type_revision_id), "TypeRevisionId",         NULL}, \
	{ACPI_RSD_UINT16,   ACPI_RSD_OFFSET (common_serial_bus.type_data_length), "TypeDataLength",         NULL}, \
	{ACPI_RSD_SOURCE,   ACPI_RSD_OFFSET (common_serial_bus.resource_source), "ResourceSource",          NULL}, \
	{ACPI_RSD_UINT16,   ACPI_RSD_OFFSET (common_serial_bus.vendor_length),  "VendorLength",             NULL}, \
	{ACPI_RSD_SHORTLISTX,ACPI_RSD_OFFSET (common_serial_bus.vendor_data),   "VendorData",               NULL},

struct acpi_rsdump_info acpi_rs_dump_common_serial_bus[10] = {
	{ACPI_RSD_TITLE, ACPI_RSD_TABLE_SIZE(acpi_rs_dump_common_serial_bus),
	 "Common Serial Bus", NULL},
	ACPI_RS_DUMP_COMMON_SERIAL_BUS
};

struct acpi_rsdump_info acpi_rs_dump_i2c_serial_bus[13] = {
	{ACPI_RSD_TITLE, ACPI_RSD_TABLE_SIZE(acpi_rs_dump_i2c_serial_bus),
	 "I2C Serial Bus", NULL},
	ACPI_RS_DUMP_COMMON_SERIAL_BUS {ACPI_RSD_1BITFLAG,
					ACPI_RSD_OFFSET(i2c_serial_bus.
							access_mode),
					"AccessMode", acpi_gbl_am_decode},
	{ACPI_RSD_UINT32, ACPI_RSD_OFFSET(i2c_serial_bus.connection_speed),
	 "ConnectionSpeed", NULL},
	{ACPI_RSD_UINT16, ACPI_RSD_OFFSET(i2c_serial_bus.slave_address),
	 "SlaveAddress", NULL},
};

struct acpi_rsdump_info acpi_rs_dump_spi_serial_bus[17] = {
	{ACPI_RSD_TITLE, ACPI_RSD_TABLE_SIZE(acpi_rs_dump_spi_serial_bus),
	 "Spi Serial Bus", NULL},
	ACPI_RS_DUMP_COMMON_SERIAL_BUS {ACPI_RSD_1BITFLAG,
					ACPI_RSD_OFFSET(spi_serial_bus.
							wire_mode), "WireMode",
					acpi_gbl_wm_decode},
	{ACPI_RSD_1BITFLAG, ACPI_RSD_OFFSET(spi_serial_bus.device_polarity),
	 "DevicePolarity", acpi_gbl_dp_decode},
	{ACPI_RSD_UINT8, ACPI_RSD_OFFSET(spi_serial_bus.data_bit_length),
	 "DataBitLength", NULL},
	{ACPI_RSD_UINT8, ACPI_RSD_OFFSET(spi_serial_bus.clock_phase),
	 "ClockPhase", acpi_gbl_cph_decode},
	{ACPI_RSD_UINT8, ACPI_RSD_OFFSET(spi_serial_bus.clock_polarity),
	 "ClockPolarity", acpi_gbl_cpo_decode},
	{ACPI_RSD_UINT16, ACPI_RSD_OFFSET(spi_serial_bus.device_selection),
	 "DeviceSelection", NULL},
	{ACPI_RSD_UINT32, ACPI_RSD_OFFSET(spi_serial_bus.connection_speed),
	 "ConnectionSpeed", NULL},
};

struct acpi_rsdump_info acpi_rs_dump_uart_serial_bus[19] = {
	{ACPI_RSD_TITLE, ACPI_RSD_TABLE_SIZE(acpi_rs_dump_uart_serial_bus),
	 "Uart Serial Bus", NULL},
	ACPI_RS_DUMP_COMMON_SERIAL_BUS {ACPI_RSD_2BITFLAG,
					ACPI_RSD_OFFSET(uart_serial_bus.
							flow_control),
					"FlowControl", acpi_gbl_fc_decode},
	{ACPI_RSD_2BITFLAG, ACPI_RSD_OFFSET(uart_serial_bus.stop_bits),
	 "StopBits", acpi_gbl_sb_decode},
	{ACPI_RSD_3BITFLAG, ACPI_RSD_OFFSET(uart_serial_bus.data_bits),
	 "DataBits", acpi_gbl_bpb_decode},
	{ACPI_RSD_1BITFLAG, ACPI_RSD_OFFSET(uart_serial_bus.endian), "Endian",
	 acpi_gbl_ed_decode},
	{ACPI_RSD_UINT8, ACPI_RSD_OFFSET(uart_serial_bus.parity), "Parity",
	 acpi_gbl_pt_decode},
	{ACPI_RSD_UINT8, ACPI_RSD_OFFSET(uart_serial_bus.lines_enabled),
	 "LinesEnabled", NULL},
	{ACPI_RSD_UINT16, ACPI_RSD_OFFSET(uart_serial_bus.rx_fifo_size),
	 "RxFifoSize", NULL},
	{ACPI_RSD_UINT16, ACPI_RSD_OFFSET(uart_serial_bus.tx_fifo_size),
	 "TxFifoSize", NULL},
	{ACPI_RSD_UINT32, ACPI_RSD_OFFSET(uart_serial_bus.default_baud_rate),
	 "ConnectionSpeed", NULL},
};

/*
 * Tables used for common address descriptor flag fields
 */
struct acpi_rsdump_info acpi_rs_dump_general_flags[5] = {
	{ACPI_RSD_TITLE, ACPI_RSD_TABLE_SIZE(acpi_rs_dump_general_flags), NULL,
	 NULL},
	{ACPI_RSD_1BITFLAG, ACPI_RSD_OFFSET(address.producer_consumer),
	 "Consumer/Producer", acpi_gbl_consume_decode},
	{ACPI_RSD_1BITFLAG, ACPI_RSD_OFFSET(address.decode), "Address Decode",
	 acpi_gbl_dec_decode},
	{ACPI_RSD_1BITFLAG, ACPI_RSD_OFFSET(address.min_address_fixed),
	 "Min Relocatability", acpi_gbl_min_decode},
	{ACPI_RSD_1BITFLAG, ACPI_RSD_OFFSET(address.max_address_fixed),
	 "Max Relocatability", acpi_gbl_max_decode}
};

struct acpi_rsdump_info acpi_rs_dump_memory_flags[5] = {
	{ACPI_RSD_LITERAL, ACPI_RSD_TABLE_SIZE(acpi_rs_dump_memory_flags),
	 "Resource Type", (void *)"Memory Range"},
	{ACPI_RSD_1BITFLAG, ACPI_RSD_OFFSET(address.info.mem.write_protect),
	 "Write Protect", acpi_gbl_rw_decode},
	{ACPI_RSD_2BITFLAG, ACPI_RSD_OFFSET(address.info.mem.caching),
	 "Caching", acpi_gbl_mem_decode},
	{ACPI_RSD_2BITFLAG, ACPI_RSD_OFFSET(address.info.mem.range_type),
	 "Range Type", acpi_gbl_mtp_decode},
	{ACPI_RSD_1BITFLAG, ACPI_RSD_OFFSET(address.info.mem.translation),
	 "Translation", acpi_gbl_ttp_decode}
};

struct acpi_rsdump_info acpi_rs_dump_io_flags[4] = {
	{ACPI_RSD_LITERAL, ACPI_RSD_TABLE_SIZE(acpi_rs_dump_io_flags),
	 "Resource Type", (void *)"I/O Range"},
	{ACPI_RSD_2BITFLAG, ACPI_RSD_OFFSET(address.info.io.range_type),
	 "Range Type", acpi_gbl_rng_decode},
	{ACPI_RSD_1BITFLAG, ACPI_RSD_OFFSET(address.info.io.translation),
	 "Translation", acpi_gbl_ttp_decode},
	{ACPI_RSD_1BITFLAG, ACPI_RSD_OFFSET(address.info.io.translation_type),
	 "Translation Type", acpi_gbl_trs_decode}
};

/*
 * Table used to dump _PRT contents
 */
struct acpi_rsdump_info acpi_rs_dump_prt[5] = {
	{ACPI_RSD_TITLE, ACPI_RSD_TABLE_SIZE(acpi_rs_dump_prt), NULL, NULL},
	{ACPI_RSD_UINT64, ACPI_PRT_OFFSET(address), "Address", NULL},
	{ACPI_RSD_UINT32, ACPI_PRT_OFFSET(pin), "Pin", NULL},
	{ACPI_RSD_STRING, ACPI_PRT_OFFSET(source[0]), "Source", NULL},
	{ACPI_RSD_UINT32, ACPI_PRT_OFFSET(source_index), "Source Index", NULL}
};

#endif
