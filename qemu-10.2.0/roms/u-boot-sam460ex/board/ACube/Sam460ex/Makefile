#
# (C) Copyright 2009-2024
# Max Tretene, ACube Systems Srl. mtretene@acube-systems.com.
#
# See file CREDITS for list of people who contributed to this
# project.
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License as
# published by the Free Software Foundation; either version 2 of
# the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place, Suite 330, Boston,
# MA 02111-1307 USA
#

include $(TOPDIR)/config.mk

CFLAGS += -I../menu/ -I../common/ -I. -I../Sam460ex/

LIB	= $(obj)lib$(BOARD).a

EMUDIR  = ../bios_emulator/scitech/src/x86emu/
EMUOBJ  = $(EMUDIR)fpu.o $(EMUDIR)ops.o ../bios_emulator/x86interface.o  \
		../bios_emulator/bios.o ../bios_emulator/glue.o

MENUDIR = ../menu/
MENUOBJ = $(MENUDIR)bios_menu.o $(MENUDIR)bootselect_menu.o $(MENUDIR)list.o \
		$(MENUDIR)cmd_menu.o $(MENUDIR)label_items.o $(MENUDIR)creation.o \
		$(MENUDIR)func_items.o $(MENUDIR)layout.o \
		$(MENUDIR)menu.o $(MENUDIR)popup_items.o \
		$(MENUDIR)string_edit.o $(MENUDIR)string_items.o

COMMONDIR = ../common/
COMMONOBJ = $(COMMONDIR)init_radeon.o $(COMMONDIR)vesa_video.o \
		$(COMMONDIR)cmd_boota.o $(COMMONDIR)misc_utils.o $(COMMONDIR)sys_dep.o \
		$(COMMONDIR)sam_ide.o $(COMMONDIR)init_sm502.o $(COMMONDIR)sm502.o \
		$(COMMONDIR)init_permedia2.o $(COMMONDIR)permedia2.o $(COMMONDIR)nanojpeg.o

COBJS-y	:= $(BOARD).o sam460_pci.o $(COMMONOBJ) $(EMUOBJ) $(MENUOBJ)
			
SOBJS	:= init.o

COBJS   := $(COBJS-y)
SRCS	:= $(SOBJS:.o=.S) $(COBJS:.o=.c)
OBJS	:= $(addprefix $(obj),$(COBJS))
SOBJS	:= $(addprefix $(obj),$(SOBJS))

$(LIB):	$(OBJS) $(SOBJS)
	$(AR) $(ARFLAGS) $@ $(OBJS) $(SOBJS)

clean:
	rm -f $(SOBJS) $(OBJS)

distclean:	clean
	rm -f $(LIB) core *.bak $(obj).depend

#########################################################################

# defines $(obj).depend target
include $(SRCTREE)/rules.mk

sinclude $(obj).depend

#########################################################################
