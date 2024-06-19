build:
	mkdir -p bin
	gcc nix.c -o bin/nix --pedantic -Wall -Wextra -Werror -O3 -ffast-math

clean:
	rm -rf bin/nix