/* JTAG low-level I/O to FX2

Using I2C addresses above 0x80 in the USRP/XGUFF framework
 
Copyright (C) 2005-2011 Uwe Bonnes bon@elektron.ikp.physik.tu-darmstadt.de

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#include <errno.h>
#include <sys/time.h>

#include <xguff/usrp_interfaces.h>
#include <xguff/usrp_commands.h>
#include <string.h>
#include <cstdio>

#include "iofx2.h"
#include "io_exception.h"
#include "utilities.h"

IOFX2::IOFX2()
:  IOBase(), bptr(0), calls_rd(0) , calls_wr(0)
{
}

int IOFX2::Init(struct cable_t *cable, char const *serial, unsigned int freq)
{
    unsigned int vendor = 0xfffe, product = 0x0018;
    char descstring[256];
    char *p = cable->optstring;
    char *description = 0;
    int res;
    
  /* split string by hand for more flexibility*/
  if (p)
  {
      vendor = strtol(p, NULL, 0);
      p = strchr(p,':');
      if(p)
          p ++;
  }
  if (p)
  {
      product = strtol(p, NULL, 0);
      p = strchr(p,':');
      if(p)
          p ++;
  }
  if (p)
  {
      char *q = strchr(p,':');
      int len;

      if (q)
          len = q-p-1;
      else
          len = strlen(p);
      if (len >0)
          strncpy(descstring, p, (len>256)?256:len);
      p = q;
      if(p)
          p ++;
  }
         
  if (verbose)
  {
      fprintf(stderr, "Cable %s type %s VID 0x%04x PID %04x",
              cable->alias, getCableName(cable->cabletype), vendor, product);
      if (description)
          fprintf(stderr, " Desc %s", description);
      if (serial)
          fprintf(stderr, " Serial %s", serial);
      fprintf(stderr, "\n");
  }

  // Open device
  res = fx2_usb_open_desc(vendor, product, description, serial);
  if(res)
  {
      fprintf(stderr," Can't open device, res: %d\n", res);  
      return res;
  }
  return 0;
}

IOFX2::~IOFX2()
{
  usrp_close_interface (fx2_dev);
}

void IOFX2::txrx_block(const unsigned char *tdi, unsigned char *tdo,
		       int length, bool last)
{
  unsigned char sdummy[1]={0};
  unsigned char *tmpsbuf = ( unsigned char *)tdi;
  unsigned char *tmprbuf = tdo;
  unsigned int rem = (last)? length - 1: length;
  int i2c_write_addr;
  if (tdi && tdo)
    {
      i2c_write_addr = USRP_CLOCK_INOUT_BYTES;
      while (rem > USRP_CMD_SIZE*8) 
	{
	  usrp_i2c_write(i2c_write_addr, tmpsbuf, USRP_CMD_SIZE);
	  usrp_i2c_read(i2c_write_addr, tmprbuf, USRP_CMD_SIZE);
	  tmpsbuf+= USRP_CMD_SIZE;
	  tmprbuf+= USRP_CMD_SIZE;
	  rem = rem - USRP_CMD_SIZE*8;
	}
      if (rem/8)
	{
	  usrp_i2c_write(i2c_write_addr, tmpsbuf, rem/8);
	  usrp_i2c_read(i2c_write_addr, tmprbuf, rem/8);
	  tmpsbuf+= rem/8;
	  tmprbuf+= rem/8;
	  rem = rem%8;
	}
      if (last)
	rem++;
      if (last)
	i2c_write_addr = USRP_CLOCK_INOUT_BITS_LAST;
      else
	i2c_write_addr = USRP_CLOCK_INOUT_BITS;
      if(rem)
	{
	  sdummy[0] = *tmpsbuf;
	  usrp_i2c_write(i2c_write_addr, sdummy, rem);
	  usrp_i2c_read (USRP_CLOCK_INOUT_BITS, sdummy, 1);
	  *tmprbuf = sdummy[0]>>(8-rem);
	}
    }
  else if (tdi)
    {
      i2c_write_addr = USRP_CLOCK_OUT_BYTES;
      while (rem > USRP_CMD_SIZE*8) 
	{
	  usrp_i2c_write(i2c_write_addr, tmpsbuf, USRP_CMD_SIZE);
	  tmpsbuf+=USRP_CMD_SIZE;
	  rem = rem - USRP_CMD_SIZE*8;
	}
      if (rem/8)
	{
	  usrp_i2c_write(i2c_write_addr, tmpsbuf, rem/8);
	  tmpsbuf+= rem/8;
	  rem = rem%8;
	}
      if (last)
	rem++;
      if (last)
	i2c_write_addr = USRP_CLOCK_INOUT_BITS_LAST;
      else
	i2c_write_addr = USRP_CLOCK_INOUT_BITS;
      if(rem)
	{
	  sdummy[0] = *tmpsbuf;
	  usrp_i2c_write(i2c_write_addr, sdummy, rem);
	}
    }
  else
    {
      i2c_write_addr = USRP_CLOCK_IN_BYTES;
      while (rem > USRP_CMD_SIZE*8) 
	{
	  usrp_i2c_read(i2c_write_addr, tmprbuf, USRP_CMD_SIZE);
	  tmprbuf+=USRP_CMD_SIZE;
	  rem = rem - USRP_CMD_SIZE*8;
	}
      if (rem/8)
	{
	  usrp_i2c_read(i2c_write_addr, tmprbuf, rem/8);
	  tmprbuf+= rem/8;
	  rem = rem%8;
	}
      if (last)
	rem++;
      if (last)
	i2c_write_addr = USRP_CLOCK_INOUT_BITS_LAST;
      else
	i2c_write_addr = USRP_CLOCK_INOUT_BITS;
      if(rem)
	{
	  sdummy[0] = 0;
	  usrp_i2c_write(i2c_write_addr, sdummy, rem);
	  usrp_i2c_read (USRP_CLOCK_INOUT_BITS, sdummy, 1);
	  *tmprbuf = sdummy[0]>>(8-rem);
	}
    }
}

void IOFX2::tx_tms(unsigned char *pat, int length, int force)
{
  if (length > USRP_CMD_SIZE*8)
    fprintf(stderr, "ToDo: Break up long TMS sequences\n");
  usrp_i2c_write( USPR_CLOCK_OUT_TMS, pat, length);
}

#define fx2_error_return(code, str) do {  \
        error_str = str;                  \
        return code;                      \
   } while(0);

int IOFX2::fx2_usb_open_desc(int vendor, int product, const char* description,
			     const char* serial)
{
  /* Adapted from libftdi:ftdi_usb_open_desc()
     
     Opens the first device with a given, vendor id, product id,
     description and serial. 
  */
  
  struct usb_bus *bus;
  struct usb_device *dev;
  char string[256];
  
  usb_init();
  
  if (usb_find_busses() < 0)
       fx2_error_return(-1, "usb_find_busses() failed");
   if (usb_find_devices() < 0)
     fx2_error_return(-2, "usb_find_devices() failed");
  
  for (bus = usb_get_busses(); bus; bus = bus->next) 
    {
      for (dev = bus->devices; dev; dev = dev->next) 
	{
	  if (dev->descriptor.idVendor == vendor
	      && dev->descriptor.idProduct == product) 
	    {
	      if (!(fx2_dev = usb_open(dev)))
		fx2_error_return(-4, "usb_open() failed");
	      if (description != NULL) 
		{
		  if (usb_get_string_simple(fx2_dev, dev->descriptor.iProduct,
					    string, sizeof(string)) <= 0) 
		    {
		      usb_close (fx2_dev);
		      fx2_error_return(-8, 
				       "unable to fetch product description");
		    }
		  if (strncmp(string, description, sizeof(string)) != 0) 
		    {
		      if (usb_close (fx2_dev) != 0)
			fx2_error_return(-10, "unable to close device");
		      continue;
		    }
		}
	      
	      if (serial != NULL) 
		{
		  if (usb_get_string_simple(fx2_dev, 
					    dev->descriptor.iSerialNumber, 
					    string, sizeof(string)) <= 0) 
		    {
		      usb_close (fx2_dev);
		      fx2_error_return(-9, "unable to fetch serial number");
		    }
		  if (strncmp(string, serial, sizeof(string)) != 0) 
		    {
		      if (usb_close (fx2_dev) != 0)
			fx2_error_return(-10, "unable to close device");
		      continue;
		    }
		  
		}
	      
	      if (usb_close (fx2_dev) != 0)
		fx2_error_return(-10, "unable to close device");
	      
	      fx2_dev = usrp_open_interface (dev, USRP_CMD_INTERFACE,
					     USRP_CMD_ALTINTERFACE);
	      if (fx2_dev)
		return 0;
	      else
		fx2_error_return(-4, "usb_open() failed");
	    }
	}
    }
  // device not found
  fx2_error_return(-3, "device not found");
}

struct 
usb_dev_handle * IOFX2::usrp_open_interface (struct usb_device *dev,
					     int interface, int altinterface)
{
  /* Taken von usrp usrp_prims.cc*/

  struct usb_dev_handle *udh = usb_open (dev);
  if (udh == 0)
    return 0;

  if (usb_set_configuration (udh, 1) < 0){
    fprintf (stderr, "%s: usb_claim_interface: failed conf %d\n",
	     __FUNCTION__,interface);
    fprintf (stderr, "%s\n", usb_strerror());
    usb_close (udh);
    return 0;
  }

  if (usb_claim_interface (udh, interface) < 0){
    fprintf (stderr, "%s:usb_claim_interface: failed interface %d\n",
	     __FUNCTION__,interface);
    fprintf (stderr, "%s\n", usb_strerror());
    usb_close (udh);
    return 0;
  }

  if (usb_set_altinterface (udh, altinterface) < 0){
    fprintf (stderr, "%s:usb_set_alt_interface: failed\n", __FUNCTION__);
    fprintf (stderr, "%s\n", usb_strerror());
    usb_release_interface (udh, interface);
    usb_close (udh);
    return 0;
  }

  return udh;
}

bool IOFX2::usrp_close_interface (struct usb_dev_handle *udh)
{
  // we're assuming that closing an interface automatically releases it.
  if(verbose)
    fprintf(stderr, "USB Read Transactions: %d USB Write Transactions %d\n", 
	   calls_rd, calls_wr);
  return usb_close (udh) == 0;
}

bool IOFX2::usrp_i2c_write(int i2c_addr, const void *buf, int len)
{
  if (len < 1 || len > MAX_EP0_PKTSIZE)
    return false;
  calls_wr++;

  //int i;
  //  fprintf(stderr, "usrp_i2c_write Addr 0x%02x len %d: ", i2c_addr, len);
  //for(i=0; i<len; i++)
  //  fprintf(stderr, " %02x", ((const unsigned char *)buf)[i]);
  //fprintf(stderr, "\n");

  return write_cmd (fx2_dev, VRQ_I2C_WRITE, i2c_addr, 0,
                    (unsigned char *) buf, len) == len;
}

bool IOFX2::usrp_i2c_read (int i2c_addr, void *buf, int len)
{
  bool ret;
  if (len < 1 || len > MAX_EP0_PKTSIZE)
    return false;
  calls_rd++;

  ret = write_cmd (fx2_dev, VRQ_I2C_READ, i2c_addr, 0,
		   (unsigned char *) buf, len) == len;
  //int i;
  //fprintf(stderr, "usrp_i2c_read  Addr 0x%02x len %d: ", i2c_addr, len);
  //for(i=0; i<len; i++)
  //  fprintf(stderr, " %02x", ((unsigned char *)buf)[i]);
  //fprintf(stderr, "\n");

  return ret;
}

int  IOFX2::write_cmd (struct usb_dev_handle *udh,
           int request, int value, int index,
           unsigned char *bytes, int len)
{
  int   requesttype = (request & 0x80) ? VRT_VENDOR_IN : VRT_VENDOR_OUT;

  int r = usb_control_msg (udh, requesttype, request, value, index,
                           (char *) bytes, len, 1000);
  if (r < 0){
    if (errno  == ENXIO) 
      throw  io_exception(std::string("Device probably disconnected, Aborting!"));
    // we get EPIPE if the firmware stalls the endpoint.
    if (errno != EPIPE)
      {
	fprintf (stderr, "usb_control_msg failed: %s\n", usb_strerror ());
	fprintf (stderr, "Perhaps the cable is bad!\n");
      }
  }
  return r;
}
  
