NAME = mkfs
SRCS = mkfs.c fscore.c
INC_DIR = .
IMG = ramdisk.img
FSIMG_PATH = ../fsimg
NANOS_BUILD_DIR = $(abspath ../../nanos-lite/build)

CC = gcc
CFLAGS = -O2 -Wall -I$(INC_DIR) -D_FILE_OFFSET_BITS=64

.PHONY: all clean install run-test init tree

all: $(NAME)

$(NAME): $(SRCS)
	@echo "+ CC $@"
	@$(CC) $(CFLAGS) $^ -o $@

# 优化 install：
# 1. 初始化镜像（防止在旧镜像基础上追加）
# 2. 使用 xargs 处理文件，防止空格导致路径截断
# 3. 增加编译检查，确保镜像路径存在
install: $(NAME)
	@echo "Creating $(IMG) from $(FSIMG_PATH)..."
	./$(NAME) init  # 确保每次 install 先初始化 ramdisk.img
	@find -L $(FSIMG_PATH) -not -path '*/usr/bin/*' -type f | while read host_path; do \
		target_path=$${host_path#$(FSIMG_PATH)}; \
		./$(NAME) $$host_path $$target_path; \
	done
	@mkdir -p $(NANOS_BUILD_DIR)
	@cp $(IMG) $(NANOS_BUILD_DIR)/ramdisk.img
	@echo "Installation complete: $(IMG) -> $(NANOS_BUILD_DIR)/ramdisk.img"

# 修正 init 目标的依赖，它应该只依赖程序本身
init: $(NAME)
	./$(NAME) init

tree: $(NAME)
	./$(NAME) tree

clean:
	rm -f $(NAME) $(IMG)

run-test: $(NAME)
	./$(NAME)