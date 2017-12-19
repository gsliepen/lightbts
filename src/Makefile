all: lbts

CFLAGS ?= -Wall -O0 -g
CFLAGS += -std=c++11
CFLAGS += `pkg-config --cflags mimesis sqlite3`
LIBS += -lfmt -lboost_filesystem -lboost_system
LIBS += `pkg-config --libs mimesis sqlite3`

%.o: %.cpp
	$(CXX) $(CFLAGS) -c -o $@ $<
lbts: cli.o list.o lightbts.o config.o
	$(CXX) $(CFLAGS) -o $@ $^ $(LIBS)

clean:
	rm -rf *.o lbts

.PHONY: clean
