build:
	mkdir -p bin
	gcc nix.c -o bin/nix --pedantic -Wall -Wextra -Werror