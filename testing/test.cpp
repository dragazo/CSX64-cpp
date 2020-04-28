#include <iostream>
#include <stdexcept>

void type_tests();
void asm_tests();

int main() try
{
	type_tests();
	asm_tests();

	return 0;
}
catch (const std::exception &ex)
{
	std::cerr << "\n\nUNHANDLED EXCEPTION: " << ex.what() << '\n';
	return 666;
}
catch (...)
{
	std::cerr << "\n\nUNHANDLED EXCEPTION OF UNKNOWN TYPE\n";
	return 999;
}
