BUSIF=SDIO
MY_PWD=$(PWD)
ROM=true
CHIP=RK915

ifeq ($(CONFIG_RK915),)
CONFIG_RK915=m
endif

L_BUSIF=$(shell echo $(BUSIF) | tr A-Z a-z)

NAME     = rk915

ccflags-y += -I$(src)/inc \
                -I$(src)/shared

ccflags-y += -DDEBUG

OBJS        =   src/main.o \
		src/hal.o \
		src/umac_if.o \
		src/rpu_if.o \
		src/tx.o \
		src/rx.o \
		src/beacon.o \
		src/p2p.o \
		src/pktgen.o \
		src/procfs.o \
		src/utils.o \
		src/vif.o \
		src/wow.o \
		src/soc.o \
		src/hal_io.o \
		src/platform.o \
		src/firmware.o \
		src/init.o

## To enable sleep functionality
## we need to enable ROM
ifeq ($(ROM),true)
ccflags-y += -DRPU_SLEEP_ENABLE
#ccflags-y += -DPS_SLEEP_TEST
#ccflags-y += -DRPU_NO_SLEEP_FLAG
#ccflags-y += -DRPU_ENABLE_PS
else
ccflags-y += -DNO_HP_READY_WAR
endif
# BUSIF
ifeq ($(BUSIF),SDIO)
ccflags-y += -DHAL_SDIO
OBJS	   +=	src/sdio.o
endif

#ccflags-y += -DSKIP_DL_FW
ifeq ($(CHIP),RK915)
ccflags-y += -DRK915
endif

#ccflags-y += -DRK3036_DONGLE
#ccflags-y += -DSTA_AP_COEXIST
ccflags-y += -DSDIO_CLOCK_SWITCH

GCC_VER_49 := $(shell echo `$(CC) -dumpversion | cut -f1-2 -d.` \>= 4.9 | bc )
ifeq ($(GCC_VER_49),1)
ccflags-y += -Wno-date-time  # Fix compile error && warning on gcc 4.9 and later
#ccflags-y += -Wno-error=date-time
endif

obj-$(CONFIG_RK915) += $(NAME).o
$(NAME)-objs= $(OBJS)

ifneq ($(KERNELRELEASE),)

#EXTRA_LDFLAGS += --strip-debug

else

all:
	@echo "BUSIF is $(L_BUSIF)"
	@echo "Compiling for $(BUSIF)"
	@make -C $(KROOT) M=$(MY_PWD) modules
clean:
	@make -C $(KROOT) M=$(MY_PWD) clean
	rm -f src/*.o

endif

