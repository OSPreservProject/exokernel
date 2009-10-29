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

/*include files for IDE drive (exopc/sys/dev/ata-{all,disk,dma}.c)*/
#define NPCI 1
#define NATADISK 4
#define NATAPICD 0
#define NATAPIFD 0
#define NATAPIST 0
#define NAPM -1
#define NISA 0
#define SMP 0


typedef struct pci_dev *device_t; 
#define       TAILQ_FIRST(head) ((head)->tqh_first)
#define vtophys(x) kva2pa(x)
#define splbio() 1
#undef splx
static void inline splx (int x) {}
#define malloc(a,b,c) malloc(a)
#define free(a,b) free(a)
#define wakeup(x)
#define DELAY delay

#define MALLOC_DEFINE(x,y,z)


#define PCIP_STORAGE_IDE_MASTERDEV      0x80
static const int bootverbose = 1;

struct buf_queue_head {
	TAILQ_HEAD(buf_queue, buf) queue;
	daddr_t	last_pblkno;
	struct	buf *insert_point;
	struct	buf *switch_point;
};


static inline void pci_write_config(device_t dev, int reg, unsigned int value, int width) {
  switch(width) {
  case 1:
    pcibios_write_config_byte(dev->bus->number, dev->devfn, reg, (unsigned char)value);
    return;
  case 2:
    pcibios_write_config_word(dev->bus->number, dev->devfn, reg, (unsigned short)value);
    return;
  case 4:
  default:
    pcibios_write_config_dword(dev->bus->number, dev->devfn, reg, value);
    return;
  }
}
static inline unsigned int pci_read_config(device_t dev, int reg, int width) {
  unsigned int ireturn;
  unsigned short sreturn;
  unsigned char creturn;
  switch(width) {
  case 2:
    pcibios_read_config_word(dev->bus->number, dev->devfn, (unsigned char)reg, &sreturn);
    return sreturn;
  case 1:
    pcibios_read_config_byte(dev->bus->number, dev->devfn, (unsigned char)reg, &creturn);
    return creturn;
  case 4:
  default:
    pcibios_read_config_dword(dev->bus->number, dev->devfn, (unsigned char)reg, &ireturn);
    return ireturn;
  }
}
    
    
    

