CC      := gcc
CFLAGS  := -O2 -Wall -Wextra -std=c11
TARGET  := paging
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
	./$(TARGET) -r "1 2 3 4 1 2 5 1 2 3 4 5" -n 3

test: $(TARGET)
	@echo "===== case 1 (n=3) ====="
	./$(TARGET) -f cases/case1.txt -n 3
	@echo ""
	@echo "===== case 1 (n=4) ====="
	./$(TARGET) -f cases/case1.txt -n 4

clean:
	rm -rf $(OBJDIR) $(TARGET)
