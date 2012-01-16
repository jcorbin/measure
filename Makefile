CFLAGS=-std=c1x -D_GNU_SOURCE
LIBS=-lrt

measure_obj=childcomm.o program.o measure.o

measure: $(measure_obj)
	gcc -o $@ $^ $(CFLAGS) $(LIBS)

.PHONY: clean

clean:
	rm measure $(measure_obj) 2>/dev/null || true
