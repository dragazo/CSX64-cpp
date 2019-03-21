release:
	g++ -o csx.exe -O4 -Wpedantic -Wreturn-type -std=c++17 *.cpp -lstdc++fs

debug:
	g++ -o csx.exe -std=c++17 *.cpp -lstdc++fs
