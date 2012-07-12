# bcm4329
DHDCFLAGS = -fshort-wchar -DLINUX  -DBCMDRIVER -DBCMDONGLEHOST	\
	-DDHDTHREAD -DBCMWPA2 -DBCMWAPI_WPI -DUNRELEASEDCHIP	\
	-DDHD_GPL -DDHD_SCHED -DDHD_DEBUG -DSDTEST -DBDC -DTOE	\
	-DDHD_BCMEVENTS -DSHOW_EVENTS -Dlinux -DBCMDBG	\
	-DCUSTOMER_HW2 -DSOFTAP -DKEEP_ALIVE	\
	-DPNO_SUPPORT -DCONFIG_FIRST_SCAN -DOOB_INTR_ONLY	\
	-DMMC_SDIO_ABORT -DHW_OOB -DCONFIG_WIFI_CONTROL_FUNC	\
	-DDHD_SDALIGN=64 -DMAX_HDR_READ=64 -DBCMSDIO -DDHD_GPL	\
	-DBCMLXSDMMC -DBCMPLATFORM_BUS -DOEM_ANDROID	\
	-DEMBEDDED_PLATFORM -DARP_OFFLOAD_SUPPORT -DPKT_FILTER_SUPPORT -DJRD -DCSCAN	\
	-Wall -Wstrict-prototypes -Werror \
	-DSET_RANDOM_MAC_SOFTAP                                               \
	-DGET_CUSTOM_MAC_ENABLE 		\
	-I$(src)/src/include/ \
	-I$(src)/src/dhd/sys/ \
	-I$(src)/src/dongle/ \
	-I$(src)/src/bcmsdio/sys/ \
	-I$(src)/src/wl/sys/ \
	-I$(src)/src/shared/

DHDOFILES = src/dhd/sys/dhd_linux.o \
	    src/shared/linux_osl.o \
	    src/shared/bcmutils.o \
	    src/dhd/sys/dhd_common.o \
	    src/dhd/sys/dhd_custom_gpio.o \
	    src/wl/sys/wl_iw.o \
	    src/shared/siutils.o \
	    src/shared/sbutils.o \
	    src/shared/aiutils.o \
	    src/shared/hndpmu.o \
	    src/shared/bcmwifi.o \
	    src/dhd/sys/dhd_sdio.o \
	    src/dhd/sys/dhd_linux_sched.o \
	    src/dhd/sys/dhd_cdc.o \
	    src/bcmsdio/sys/bcmsdh_sdmmc.o \
	    src/bcmsdio/sys/bcmsdh.o \
	    src/bcmsdio/sys/bcmsdh_linux.o \
	    src/bcmsdio/sys/bcmsdh_sdmmc_linux.o
	    
obj-$(CONFIG_BCM4329) += bcm4329.o
bcm4329-objs += $(DHDOFILES)
EXTRA_CFLAGS = $(DHDCFLAGS)
EXTRA_LDFLAGS += --strip-debug
