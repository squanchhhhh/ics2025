CROSS_COMPILE = riscv64-linux-gnu-
LNK_ADDR = 0x40000000
CFLAGS  += -fno-pic -march=rv64g -mcmodel=medany
LDFLAGS += --no-relax -Ttext-segment $(LNK_ADDR)
