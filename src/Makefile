all: lbts

CFLAGS ?= -Wall -O0 -g
LIBS += -lfmt

%.o: %.cpp
	$(CXX) $(CFLAGS) -c -o $@ $<
lbts: cli.o
	$(CXX) $(CFLAGS) -o $@ $^ $(LIBS)

clean:
	rm -rf *.o lbts

.PHONY: clean
