CC      := gcc
CFLAGS  := -O2 -Wall -Wextra -std=c11
TARGET  := partition
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
	@echo "===== FF ====="
	./$(TARGET) -a ff -f cases/case1.txt
	@echo "===== BF ====="
	./$(TARGET) -a bf -f cases/case1.txt
	@echo "===== WF ====="
	./$(TARGET) -a wf -f cases/case1.txt

clean:
	rm -rf $(OBJDIR) $(TARGET)
