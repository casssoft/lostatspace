all: lostatspace
clean:
	rm -f *.o
	rm -f lostatspace
OBJS=racing.o OpenSimplexNoise.o

racing.o: racing.c OpenSimplexNoise.h
	gcc racing.c -c -o racing.o

OpenSimplexNoise.o: OpenSimplexNoise.h OpenSimplexNoise.h
	gcc OpenSimplexNoise.c -c -o OpenSimplexNoise.o

lostatspace: $(OBJS)
	gcc racing.o OpenSimplexNoise.o -lm -o lostatspace

