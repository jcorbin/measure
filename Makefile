CFLAGS=-std=c1x -D_GNU_SOURCE -g
LIBS=-lrt

measure_obj=childcomm.o program.o child.o measure.o sighandler.o

measure: $(measure_obj)
	gcc -o $@ $^ $(CFLAGS) $(LIBS)

all: measure

.PHONY: all clean

clean:
	rm measure $(measure_obj) 2>/dev/null || true
