NAME = mkfs
SRCS = mkfs.c fscore.c
INC_DIR = .
IMG = ramdisk.img
FSIMG_PATH = ../fsimg

CC = gcc
CFLAGS = -O2 -Wall -I$(INC_DIR) -D_FILE_OFFSET_BITS=64

.PHONY: all clean install run-test

all: $(NAME)

$(NAME): $(SRCS)
	$(CC) $(CFLAGS) $^ -o $@

install: $(NAME)
	@find -L $(FSIMG_PATH) -type f | while read host_path; do \
		target_path=$${host_path#$(FSIMG_PATH)}; \
		./$(NAME) $$host_path $$target_path; \
	done
	cp $(IMG) /home/parallels/ics2025/nanos-lite/build/ramdisk.img
	@echo "Installation complete."

init: $(install)
	./$(NAME) init

tree: $(NAME)
	./$(NAME) tree

clean:
	rm -f $(NAME) $(IMG)

run-test: $(NAME)
	./$(NAME)