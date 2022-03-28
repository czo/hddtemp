/*
 * Copyright (C) 2002  Emmanuel VARAGNAT <hddtemp@guzu.net>
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
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */


// Include file generated by ./configure
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

// Gettext includes
#if ENABLE_NLS
#include <libintl.h>
#define _(String) gettext (String)
#else
#define _(String) (String)
#endif

// Standard includes
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <linux/hdreg.h>

// Application specific includes
#include "hddtemp.h"
#include "atacmds.h"


#define SBUFF_SIZE 512
#define swapb(x) \
({ \
        u16 __x = (x); \
        (x) = ((u16)( \
                (((u16)(__x) & (u16)0x00ffU) << 8) | \
                (((u16)(__x) & (u16)0xff00U) >> 8) )); \
})

static char sbuff[SBUFF_SIZE];

static int ata_probe(int device) {
  if(device == -1 || ioctl(device, HDIO_GET_IDENTITY, sbuff))
    return 0;
  else
    return 1;
}

static const char *ata_model (int device) {
  if(device == -1 || ioctl(device, HDIO_GET_IDENTITY, sbuff))
    return strdup(_("unknown"));
  else
    return strdup((char*) ((u16*)sbuff + 27));
}

static const char *ata_serial (int device) {
  return strdup(_("unknown"));
}


static enum e_gettemp ata_get_temperature(struct disk *dsk) {   
  unsigned char    values[512]/*, thresholds[512]*/;
  unsigned char *  field;
  int              i;
  u16 *            p;

  if(dsk->db_entry->attribute_id == 0) {
    close(dsk->fd);
    dsk->fd = -1;
    return GETTEMP_NOSENSOR;
  }

  if(ata_get_packet(dsk->fd)) {
    snprintf(dsk->errormsg, MAX_ERRORMSG_SIZE, _("S.M.A.R.T. not available"));
    return GETTEMP_NOT_APPLICABLE;
  }

  switch(ata_get_powermode(dsk->fd)) {
  case PWM_STANDBY:
  case PWM_SLEEPING:
    if (!wakeup)
      return GETTEMP_DRIVE_SLEEP;
  case PWM_UNKNOWN:
  case PWM_ACTIVE: /* active or idle */
  default:
    break;
  }

  /* get SMART values */
  if(ata_enable_smart(dsk->fd) != 0) {
    enum e_gettemp ret;
    if(errno == EIO) {
      snprintf(dsk->errormsg, MAX_ERRORMSG_SIZE, _("S.M.A.R.T. not available"));
      ret = GETTEMP_NOT_APPLICABLE;
    }
    else
    {
      snprintf(dsk->errormsg, MAX_ERRORMSG_SIZE, "%s", strerror(errno));
      ret = GETTEMP_ERROR;
    }
    close(dsk->fd);
    dsk->fd = -1;
    return ret;
  }

  if(ata_get_smart_values(dsk->fd, values)) {
    snprintf(dsk->errormsg, MAX_ERRORMSG_SIZE, "%s", strerror(errno));
    close(dsk->fd);
    dsk->fd = -1;
    return GETTEMP_ERROR;
  }

  p = (u16*)values;
  for(i = 0; i < 256; i++) {
    swapb(*(p+i));
  }

  /* get SMART threshold values */
  /*
  if(get_smart_threshold_values(fd, thresholds)) {
    perror("ioctl");
    exit(3);
  }

  p = (u16*)thresholds;
  for(i = 0; i < 256; i++) {
    swapb(*(p+i));
  }
  */

  if (debug)
      ata_print_fields(values);

  /* temperature */
  field = ata_search_temperature(values, dsk->db_entry->attribute_id);
  if(!field && dsk->db_entry->attribute_id2 != 0)
    field = ata_search_temperature(values, dsk->db_entry->attribute_id2);

  if(field)
    dsk->value = *(field+3);

  if(dsk->value != -1)
    return GETTEMP_KNOWN;
  else
    return GETTEMP_UNKNOWN;

  /* never reached */
}


/*******************************
 *******************************/

struct bustype ata_bus = {
  "PATA",
  ata_probe,
  ata_model,
  ata_get_temperature,
  ata_serial
};
