/* driver_table.c
 *
 * author: Michael Scheinholtz
 * email:  mseu@ece.cmu.edu
 *
 * description:  
 *    This file holds a table of device drivers that have been implemented
 * specifically for the Exokernel called the driver arbitration table.  
 *    A driver in this table overrides a driver
 * contained in the OS-kit- because drivers implemented specifically for
 * the Exokernel should be more suitable for use than the same driver from
 * the OS-kit.  This table is setup statically during compile time.
 *
 */

#include <xok/driver_table.h>
#include <xok_include/assert.h>
#include <xok_include/string.h>


/***--- Local structures ---***/

/*
 * Driver structure.  right now all it has is the name.
 */
typedef struct driver_struct
{
  char *name;
} driver_t;



/***--- local global data ---***/

/*
 * An array of driver structures terminated with a null
 * Driver structure. If a driver is present in the table,
 * that means the Exokernel has a native version of the 
 * driver and the OS-kit should not use its version.
 */

static driver_t exo_driver_table[] = {{"tulip"}, {"de4x5"}, {"smc-ultra"}, {""}};



/***--- external functions ---***/


/* int is_driver_present:
 *
 * returns a 1 if the driver is in the exo_driver_table (and the
 * OS-kit should not probe that driver) 
 *
 * returns a zero if the driver is not present.
 */

int driver_is_present
   (
    char *driver_name /* the name of the driver. ie "tulip" */
   )
{
  int i; 
  int found_driver;   /* flag, 1 if driver found, 0 if not */

  /*
   * check input parameter.
   */

  assert(driver_name != NULL);

  /* 
   * search the table for a match to driver name, 
   * the end of the table must be marked by a null string.
   */
  found_driver = 0;
  for(i = 0; strncmp(exo_driver_table[i].name, "", 1) != 0; i++)
	{
	  int len = strlen(exo_driver_table[i].name);

	  if(strncmp(exo_driver_table[i].name, driver_name, len) == 0)
		{
		  found_driver = 1;
		}
	}

  return found_driver;  
}

void init_driver_table()
{
/*
 * depending on how the table is set up at compile time,
 * this function will initialize it and make sure that it
 * is ready for use by the is_driver_present_function.
 */


  /*
   * right now do nothing.
   */

}

