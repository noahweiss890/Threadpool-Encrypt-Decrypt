.PHONY: all
all: task stdinExample

task:	codec.h basic_main.c
	gcc basic_main.c ./libCodec.so -o encoder

stdinExample:	stdin_main.c
		gcc stdin_main.c ./libCodec.so -o tester

myMain:	my_main.c
		gcc my_main.c ./libCodec.so -o myMain -pthread -g

.PHONY: clean
clean:
	-rm encoder tester libCodec.so 2>/dev/null
