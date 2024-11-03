build:
	gcc -o ./bin/lox ./src/*.c

run:
	gcc -o ./bin/lox ./src/*.c
	./bin/lox
