CC      := gcc
CFLAGS  := -O2 -Wall -Wextra -std=c11
TARGET  := scheduler
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
	@echo "===== FCFS ====="; ./$(TARGET) -f cases/case1.txt -a fcfs
	@echo "===== SJF  ====="; ./$(TARGET) -f cases/case1.txt -a sjf
	@echo "===== SRTF ====="; ./$(TARGET) -f cases/case1.txt -a srtf
	@echo "===== RR q=2 ==="; ./$(TARGET) -f cases/case1.txt -a rr -q 2
	@echo "===== PRIO ====="; ./$(TARGET) -f cases/case1.txt -a prio

clean:
	rm -rf $(OBJDIR) $(TARGET)
