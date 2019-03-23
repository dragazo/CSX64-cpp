release:
	g++ -o csx.exe -O4 -Wall -std=c++17 *.cpp -lstdc++fs

debug:
	g++ -o csx.exe -O0 -Wall -std=c++17 *.cpp -lstdc++fs
