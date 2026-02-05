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
	@echo "Populating $(IMG) with files from $(FSIMG_PATH)..."
	@find $(FSIMG_PATH) -type f | while read host_path; do \
		target_path=$${host_path#$(FSIMG_PATH)}; \
		./$(NAME) $$host_path $$target_path; \
	done
	@echo "Installation complete."

clean:
	rm -f $(NAME) $(IMG)

run-test: $(NAME)
	./$(NAME)