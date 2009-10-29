/* driver_table.h
 *
 * author: Michael Scheinholtz
 * email:  mseu@ece.cmu.edu
 *
 * description:
 *   This is a header file for accessing the driver arbitration table.
 *
 *
 */

#ifndef _DRIVER_TABLE_H_
#define _DRIVER_TABLE_H_

extern int driver_is_present(char *driver_name);
extern void init_driver_table();

#endif /* DRIVER_TABLE_H */
