CC      := gcc
CFLAGS  := -O2 -Wall -Wextra -std=c11
SRCDIR  := src
OBJDIR  := build
TARGETS := mlfq_cfs realtime_demo

.PHONY: all clean test perf

all: $(TARGETS)

mlfq_cfs: $(OBJDIR)/mlfq_cfs.o
	$(CC) $(CFLAGS) -o $@ $^

realtime_demo: $(OBJDIR)/realtime_demo.o
	$(CC) $(CFLAGS) -pthread -o $@ $^

$(OBJDIR)/%.o: $(SRCDIR)/%.c | $(OBJDIR)
	$(CC) $(CFLAGS) -c -o $@ $<

$(OBJDIR):
	mkdir -p $(OBJDIR)

test: all
	@echo "===== MLFQ ====="
	./mlfq_cfs -f cases/mlfq_case.txt -a mlfq
	@echo ""
	@echo "===== CFS ====="
	./mlfq_cfs -f cases/mlfq_case.txt -a cfs
	@echo ""
	@echo "===== Compare ALL ====="
	./mlfq_cfs -f cases/mlfq_case.txt -a all
	@echo ""
	@echo "===== Real-time Demo (needs sudo) ====="
	@echo "Run: sudo ./realtime_demo fifo 4"

perf:
	@echo "Running performance benchmarks..."
	python3 scripts/perf_measure.py
	@echo "Done. See results/ for data and plots."

clean:
	rm -rf $(OBJDIR) $(TARGETS)
