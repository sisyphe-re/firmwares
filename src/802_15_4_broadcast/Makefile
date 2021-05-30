APPLICATION = 802_15_4_broadcast
OBJCOPY = arm-none-eabi-objcopy
BOARD = iotlab-m3

USEMODULE += gnrc
USEMODULE += gnrc_netdev_default
USEMODULE += auto_init_gnrc_netif

CFLAGS += -DEVENT_THREAD_HIGHEST_STACKSIZE=1024

include $(RIOTBASE)/Makefile.include
