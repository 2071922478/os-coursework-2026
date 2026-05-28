CC      := gcc
CFLAGS  := -O2 -Wall -Wextra -std=c11 -pthread
LDFLAGS := -pthread
SRCDIR  := src
OBJDIR  := build
SRCS    := $(wildcard $(SRCDIR)/*.c)
TARGETS := producer_consumer reader_writer dining_philosophers

.PHONY: all clean test

all: $(TARGETS)

producer_consumer: $(OBJDIR)/producer_consumer.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

reader_writer: $(OBJDIR)/reader_writer.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

dining_philosophers: $(OBJDIR)/dining_philosophers.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(OBJDIR)/%.o: $(SRCDIR)/%.c | $(OBJDIR)
	$(CC) $(CFLAGS) -c -o $@ $<

$(OBJDIR):
	mkdir -p $(OBJDIR)

test: all
	@echo "===== Producer-Consumer ====="
	./producer_consumer 3 3 8 10
	@echo ""
	@echo "===== Reader-Writer (RP) ====="
	./reader_writer rp 3 2 4
	@echo ""
	@echo "===== Reader-Writer (WP) ====="
	./reader_writer wp 3 2 4
	@echo ""
	@echo "===== Dining Philosophers ====="
	./dining_philosophers 5 3

clean:
	rm -rf $(OBJDIR) $(TARGETS)
