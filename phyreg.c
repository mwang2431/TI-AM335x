/*
 * phyreg.c: Simple program to read/write from/to LAN8710A PHY registers from an AM335x using MDIO
 *
 *  (c) 2017 josh.com
 *
 *  Based on:
 *  Copyright (C) 2000, Jan-Derk Bakker (jdb@lartmaker.nl)
 *
 *
 * This software has been developed for the LART computing board
 * (http://www.lart.tudelft.nl/). The development has been sponsored by
 * the Mobile MultiMedia Communications (http://www.mmc.tudelft.nl/)
 * and Ubiquitous Communications (http://www.ubicom.tudelft.nl/)
 * projects.
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

// History:
// 11-14-2023 MW 	Change to work with Marvell switch controller 88E6097F:
//			Add miiInit() function to enable MDIO control
//			Force the speed, duplex and link up for switch controller port9 to enable
//			the communication between TI335x and switch controller
//			Change main() function

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <fcntl.h>
#include <ctype.h>
#include <termios.h>
#include <sys/types.h>
#include <sys/mman.h>

#include <string.h>

/*
 * For each platform, all we need is 
 * 1) Assigning functions into 
 *         fgtReadMii : to read MII registers, and
 *         fgtWriteMii : to write MII registers.
 *
 * 2) Register Interrupt (Not Defined Yet.)
*/


#define MAP_SIZE 0x90
#define MAP_MASK (MAP_SIZE - 1)

#define MDIO_BASE_TARGET 		0x4a101000 // ARM address of the MDIO controller

#define ENABLE_CTRL_OFFSET 	 	0x04  // Enable Controller Register
#define MDIO_ALIVE_OFFSET 	 	0x08  // PHY Alive Status Register
#define MDIO_LINK_OFFSET 	 	0x0c  // PHY Link Status Register

#define MDIO_USERACCESS0_OFFSET 	0x80  // MDIO User Access Register 0

#define MDIO_USERACCESS0_GO_BIT		(1<<31)	// Set to start a transaction, check to see if transaction in progress
#define MDIO_USERACCESS0_WRITE_BIT	(1<<30)	// Set to write to the PHY register
#define MDIO_USERACCESS0_ACK_BIT	(1<<29)	// "Acknowledge. This bit is set if the PHY acknowledged the read transaction."

unsigned int 	*mdiobase = NULL;
int 			phy_address=0;

// We need this because pointer arithmetic on unsigned * goes by 4's
#define OFFSET_PTR( base , offset ) ( base + (offset/sizeof( *base) ))

// Map an ARM base address to a pointer we can use via devmem
// Must be on a page boundary

unsigned *map_base( unsigned target )  {

	if ( target != (target & ~MAP_MASK )  ) {
		fprintf( stderr , "Base address must be on boundary,.\r\n");
		return(NULL);
	}


    // Open the file descriptor

    int fd = open("/dev/mem", O_RDWR | O_SYNC);

    if (fd == -1 ) {
		fprintf( stderr , "Could not open /dev/mem (are you root?).\r\n");
		return(NULL);
    }

	// And map it

    /* Map one page */

    unsigned *map_base = mmap(0, MAP_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, target );

    if (map_base == (void *) -1) {
	    fprintf( stderr , "MMAP FAILED.\r\n");
	    close(fd);
	    return(NULL);
    }

    close(fd);		// Ok to close now http://stackoverflow.com/questions/17490033/do-i-need-to-keep-a-file-open-after-calling-mmap-on-it

    return(map_base);

}

void unmap_base( unsigned *map_base ) {
    if(munmap(map_base, MAP_SIZE) == -1) {
	    		fprintf( stderr , "unmapped failed!.\r\n");
    }
}

// Will write writeval if flag is set
// returns register contents (before write if write specified)

int accessreg( volatile unsigned *useraccessaddress , unsigned short phy_address, unsigned short reg , unsigned char writeflag , unsigned writeval )
{
	printf( "PHY addr=%2.02d REG=%2.02d : " , phy_address , reg );
	
	if ( *useraccessaddress & MDIO_USERACCESS0_GO_BIT )
	{
		printf( "WAIT ");
		while (*useraccessaddress & MDIO_USERACCESS0_GO_BIT);
	}
	else
	{
		printf( "IDLE ");
	}

	if (writeflag)
	{
		printf( "WRITE 0x%x ", *useraccessaddress);
		*useraccessaddress = MDIO_USERACCESS0_GO_BIT | MDIO_USERACCESS0_WRITE_BIT | (reg << 21) | (phy_address << 16) | writeval;	// Send the  command as defined by 14.5.10.11 in TRM		
	}
	else
	{
		*useraccessaddress = MDIO_USERACCESS0_GO_BIT | (reg << 21) | (phy_address << 16);			// Send the actual read command as defined by 14.5.10.11 in TRM
		printf( "READ 0x%x ", *useraccessaddress);
	}

	// Now wait for the MDIO transaction to complete

	while (*useraccessaddress & MDIO_USERACCESS0_GO_BIT);

	if ( *useraccessaddress & MDIO_USERACCESS0_ACK_BIT )
	{
		printf( "ACK ");
		while (*useraccessaddress & MDIO_USERACCESS0_GO_BIT);
	}
	else
	{
		printf( "NAK ");
	}
	
	int data = 	 *useraccessaddress ;			// THe bottom 16 bits are the read value
	
	printf( "Read data: 0x%x" , data );
	
	if (writeflag) 
	{
		printf( " (WROTE 0x%x) " , writeval );
	}
	printf( "\n");
	
	return data; 
	
}

int readreg( unsigned *useraccessaddress , unsigned short phy_address, unsigned short reg ) {
	return accessreg( useraccessaddress , phy_address , reg , 0, 0 );
}

int  writereg( unsigned *useraccessaddress , unsigned short phy_address, unsigned short reg , unsigned short data ) {
	return accessreg( useraccessaddress , phy_address , reg , 1, data );
}

int miiInit(void)
{
	unsigned int alivebits;
	int value = 0;

	mdiobase = map_base( MDIO_BASE_TARGET );

	if (!mdiobase) {
		fprintf( stderr ,"miiInit: mmap failed, exit.\n");
		return -1;
	}

	// write 0x410000ff to enable MDIO control
	value = *(int *)OFFSET_PTR( mdiobase , ENABLE_CTRL_OFFSET );
	printf( "MDIO Control before config: 0x%x\n" , value );

	if(value != 0x410000ff)
	{
		*OFFSET_PTR( mdiobase , ENABLE_CTRL_OFFSET ) = 0x410000ff;
	}

	// Find address of first phy
	alivebits =  *OFFSET_PTR( mdiobase , MDIO_ALIVE_OFFSET );

	printf( "Alive bits: 0x%x\n", alivebits);

	while ( phy_address < 32 && (( alivebits & (1<<phy_address ))== 0 ) )
	{
		phy_address++;
	}

	if (phy_address==32)
	{
		printf( "miiInit: No PHY found, exit!\n");
		return -1;
	}
	else
	{
		printf( "miiInit: PHY address: %d\n", phy_address);
	}
	
	// Force switch controller P9 to 100Mps Full Duplex and Link Up
	writereg( OFFSET_PTR( mdiobase , MDIO_USERACCESS0_OFFSET) , 25 , 1 , 0x003D );
	
	return 0;
}

int main(int argc, char **argv) {
	
	int 			phy_address = 0;
	int 			data;
	int 			phy_reg;
	int 			r;


	printf( "mii_test start...\n");

    if(argc < 2)
    {
        fprintf( stderr, "\nUsage:sudo mii_test [R reg ] | [W reg data ] | [phyAddress reg data]\n"
			"To read register ...\n"
        	"	phyAddress	: must specify PHY address"
			"   reg     	: if not specify, read register 0-31\n"
			"\n"
    			"To write register ...\n"
            	"	phyAddress	: must specify PHY address"
    			"   reg     	: register 0-31\n"
    			"   data    	: data in hex\n"
    			"\n"
			"To debug phy addresses & registers, no R or W \n"
            "   phyAddress 	: phy address to act upon, or scan addresses\n"
            "   reg     	: phy register to act on  (if not spoecified, 0-31 will be dumped)\n"
            "   data    	: optional data to be written to reg in hex\n\n"
            );
        exit(1);
    }
	
    if( miiInit() != 0 )
    {
    	printf( "MII initialization failed, exit...\n" );
    	return -1;
    }

	if (!strcasecmp( argv[1] , "R" ) ) // read register
	{
		if( argc == 4 )
		{
			phy_address = atoi( argv[3] );
			phy_reg = atoi( argv[4] );
			readreg( OFFSET_PTR( mdiobase , MDIO_USERACCESS0_OFFSET) , phy_address , phy_reg );
		}
		else if(argc < 4)
		{
			phy_address = atoi( argv[3] );
			// No reg specified, dump all
			for(r = 0; r < 32; r++ )
			{
				readreg( OFFSET_PTR( mdiobase , MDIO_USERACCESS0_OFFSET) , phy_address , r ) ;
			}
		}
	}
	else if(!strcasecmp( argv[1] , "W")) //write register
	{
		if( argc == 5 )
		{
			phy_address = atoi( argv[3] );
			// Find address of first phy
			phy_reg = atoi( argv[4] );
			data = (short unsigned) strtol( argv[5] , NULL , 16);
			writereg( OFFSET_PTR( mdiobase , MDIO_USERACCESS0_OFFSET) , phy_address , phy_reg , data );
		}
	}
	else
	{
		phy_address = atoi( argv[1] );
		if (argc == 2)
		{
			// No reg specified, read all registers at phy_address
			for(r = 0; r < 32; r++ )
			{
				readreg( OFFSET_PTR( mdiobase , MDIO_USERACCESS0_OFFSET) , phy_address , r ) ;
			}
		}
		else if (argc == 3)
		{
			phy_reg = atoi( argv[2] );
			// Read specified reg
			readreg( OFFSET_PTR( mdiobase , MDIO_USERACCESS0_OFFSET) , phy_address , phy_reg ) ;
		}
		else if (argc == 4)
		{
			// Write
			phy_reg = atoi( argv[2] );
			// Parse as hex
			data = (short unsigned) strtol( argv[3] , NULL , 16);
			writereg( OFFSET_PTR( mdiobase , MDIO_USERACCESS0_OFFSET) , phy_address , phy_reg , data );
		}
    }

	unmap_base( mdiobase );

    return 0;
}
