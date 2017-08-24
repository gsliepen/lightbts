all: lightbts-cli

CFLAGS ?= -Wall -O0 -g
LIBS += -lfmt

%.o: %.cc
	$(CXX) $(CFLAGS) -c -o $@ $<
lightbts-cli: cli.o
	$(CXX) $(CFLAGS) -o $@ $^ $(LIBS)

clean:
	rm -rf *.o lightbts-cli

.PHONY: clean
