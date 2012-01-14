CFLAGS=-std=c1x -D_GNU_SOURCE
LIBS=-lrt

measure_obj=measure.o program.o

measure: $(measure_obj)
	gcc -o $@ $^ $(CFLAGS) $(LIBS)

.PHONY: clean

clean:
	rm measure $(measure_obj) 2>/dev/null || true
