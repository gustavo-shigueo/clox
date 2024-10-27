build:
	gcc -o ./bin/lox.exe ./src/*.c

run:
	gcc -o ./bin/lox.exe ./src/*.c
	.\bin\lox.exe
