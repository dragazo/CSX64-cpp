release:
	g++ -o csx.exe -O4 -Wall -std=c++17 *.cpp -lstdc++fs

debug:
	g++ -o csx.exe -O0 -Wall -std=c++17 *.cpp -lstdc++fs

sanitize:
	clang++ -o csx.exe -O0 -Wall -std=c++17 *.cpp -lstdc++fs -fsanitize=undefined -fsanitize=address

sanitize-opt:
	clang++ -o csx.exe -O3 -Wall -std=c++17 *.cpp -lstdc++fs -fsanitize=undefined -fsanitize=address

