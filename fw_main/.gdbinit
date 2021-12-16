

file FLASH_RUN/dingtian_relay/dingtian_relay.elf
#ile FLASH_RUN/bridge_v4/bridge_v4.elf

target remote localhost:3333

monitor mww 0xE0042008 0x00001000

