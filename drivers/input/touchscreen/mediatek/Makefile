subdir-ccflags-y += -Werror
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/
ccflags-y += -I$(srctree)/drivers/input/touchscreen/mediatek
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/$(MTK_PLATFORM)/include
# In case the platform does NOT support this type of sensors
obj-y   +=  mtk_tpd.o
obj-y   +=  tpd_button.o
obj-y   +=  tpd_calibrate.o
obj-y   +=  tpd_debug.o
obj-y   +=  tpd_default.o
obj-y   +=  tpd_misc.o
obj-y   +=  tpd_setting.o
obj-$(CONFIG_TOUCHSCREEN_MTK_FOCALTECH_TS)	+=  focaltech_tp/
obj-$(CONFIG_TOUCHSCREEN_MTK_GT1151)	+=  GT1151/

obj-$(CONFIG_TOUCHSCREEN_MTK_TLSC6X)	+=  tlsc6x/
obj-$(CONFIG_TOUCHSCREEN_MTK_GT9XXTB_HN_NEW)	+=  gt9xxtb_hn_new/
obj-$(CONFIG_TOUCHSCREEN_MTK_GSLX680)	+=  gslx680/
obj-$(CONFIG_TOUCHSCREEN_MTK_AH1887)	+=  AH1887/
obj-$(CONFIG_TOUCHSCREEN_MTK_ATMEL_MXT336) += ATMEL_MXT336/
