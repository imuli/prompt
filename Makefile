CFLAGS=-Wall
LIBS=-lutil

objects=$(patsubst %.c, bin/%.o, $(wildcard *.c))
target=bin/prompt
all: $(target)

$(target): $(objects)
	cc $(CFLAGS) -o $@ $^ $(LIBS)

bin/%.o: %.c | bin
	cc -c $(CFLAGS) -o $@ $<

tests=$(patsubst test/%.c, bin/test-%, $(wildcard test/*.c))
test: $(tests)
	for a in $^; do $$a || exit 1; done

bin/test-%: %.o bin/test-%.o
	cc $(CFLAGS) -o $@ $^

bin/test-%.o: test/%.c | bin
	cc -c $(CFLAGS) -o $@ $<

bin:
	mkdir bin

clean:
	rm -rf bin
