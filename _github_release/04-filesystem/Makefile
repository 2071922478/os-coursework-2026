CC      := gcc
CFLAGS  := -O2 -Wall -Wextra -std=c11
TARGET  := filesystem
SRCDIR  := src
OBJDIR  := build
SRCS    := $(wildcard $(SRCDIR)/*.c)
OBJS    := $(patsubst $(SRCDIR)/%.c,$(OBJDIR)/%.o,$(SRCS))

.PHONY: all clean run test

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^

$(OBJDIR)/%.o: $(SRCDIR)/%.c | $(OBJDIR)
	$(CC) $(CFLAGS) -c -o $@ $<

$(OBJDIR):
	mkdir -p $(OBJDIR)

run: $(TARGET)
	./$(TARGET)

test: $(TARGET)
	@echo "===== File System Test ====="
	@rm -f test_disk.img
	./$(TARGET) -d test_disk.img -f cases/test_script.txt
	@rm -f test_disk.img

clean:
	rm -rf $(OBJDIR) $(TARGET) *.img
