
/*
 * Copyright (C) 1997 Massachusetts Institute of Technology 
 *
 * This software is being provided by the copyright holders under the
 * following license. By obtaining, using and/or copying this software,
 * you agree that you have read, understood, and will comply with the
 * following terms and conditions:
 *
 * Permission to use, copy, modify, distribute, and sell this software
 * and its documentation for any purpose and without fee or royalty is
 * hereby granted, provided that the full text of this NOTICE appears on
 * ALL copies of the software and documentation or portions thereof,
 * including modifications, that you make.
 *
 * THIS SOFTWARE IS PROVIDED "AS IS," AND COPYRIGHT HOLDERS MAKE NO
 * REPRESENTATIONS OR WARRANTIES, EXPRESS OR IMPLIED. BY WAY OF EXAMPLE,
 * BUT NOT LIMITATION, COPYRIGHT HOLDERS MAKE NO REPRESENTATIONS OR
 * WARRANTIES OF MERCHANTABILITY OR FITNESS FOR ANY PARTICULAR PURPOSE OR
 * THAT THE USE OF THE SOFTWARE OR DOCUMENTATION WILL NOT INFRINGE ANY
 * THIRD PARTY PATENTS, COPYRIGHTS, TRADEMARKS OR OTHER RIGHTS. COPYRIGHT
 * HOLDERS WILL BEAR NO LIABILITY FOR ANY USE OF THIS SOFTWARE OR
 * DOCUMENTATION.
 *
 * The name and trademarks of copyright holders may NOT be used in
 * advertising or publicity pertaining to the software without specific,
 * written prior permission. Title to copyright in this software and any
 * associated documentation will at all times remain with copyright
 * holders. See the file AUTHORS which should have accompanied this software
 * for a list of all copyright holders.
 *
 * This file may be derived from previously copyrighted software. This
 * copyright applies only to those changes made by the copyright
 * holders listed in the AUTHORS file. The rest of this file is covered by
 * the copyright notices, if any, listed below.
 */

/*
 * BIOS32, PCI BIOS functions and defines
 * Copyright 1994, Drew Eckhardt
 * 
 * For more information, please consult 
 * 
 * PCI BIOS Specification Revision
 * PCI Local Bus Specification
 * PCI System Design Guide
 *
 * PCI Special Interest Group
 * M/S HF3-15A
 * 5200 N.E. Elam Young Parkway
 * Hillsboro, Oregon 97124-6497
 * +1 (503) 696-2000 
 * +1 (800) 433-5177
 * 
 * Manuals are $25 each or $50 for all three, plus $7 shipping 
 * within the United States, $35 abroad.
 */

#ifndef BIOS32_H
#define BIOS32_H

/*
 * Error values that may be returned by the PCI bios.  Use
 * pcibios_strerror() to convert to a printable string.
 */
#define PCIBIOS_SUCCESSFUL		0x00
#define PCIBIOS_FUNC_NOT_SUPPORTED	0x81
#define PCIBIOS_BAD_VENDOR_ID		0x83
#define PCIBIOS_DEVICE_NOT_FOUND	0x86
#define PCIBIOS_BAD_REGISTER_NUMBER	0x87
#define PCIBIOS_SET_FAILED		0x88
#define PCIBIOS_BUFFER_TOO_SMALL	0x89

extern int pcibios_present (void);
extern void pcibios_init (void);
extern int pcibios_find_class (unsigned int class_code, unsigned short index, 
			       unsigned char *bus, unsigned char *dev_fn);
extern int pcibios_find_device (unsigned short vendor, unsigned short dev_id,
				unsigned short index, unsigned char *bus,
				unsigned char *dev_fn);
extern int pcibios_read_config_byte (unsigned char bus, unsigned char dev_fn,
				     unsigned char where, unsigned char *val);
extern int pcibios_read_config_word (unsigned char bus, unsigned char dev_fn,
				     unsigned char where, unsigned short *val);
extern int pcibios_read_config_dword (unsigned char bus, unsigned char dev_fn,
				      unsigned char where, unsigned int *val);
extern int pcibios_write_config_byte (unsigned char bus, unsigned char dev_fn,
				      unsigned char where, unsigned char val);
extern int pcibios_write_config_word (unsigned char bus, unsigned char dev_fn,
				      unsigned char where, unsigned short val);
extern int pcibios_write_config_dword (unsigned char bus, unsigned char dev_fn,
				       unsigned char where, unsigned int val);
extern const char *pcibios_strerror (int error);

#endif /* BIOS32_H */
