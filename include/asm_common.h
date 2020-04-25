#ifndef CSX64_ASM_COMMON_H
#define CSX64_ASM_COMMON_H

#include <string>

namespace CSX64
{
	/// <summary>
	/// Converts a string token into its character internals (accouting for C-style escapes in the case of `backquotes`)
	/// </summary>
	/// <param name="token">the string token to process (with quotes around it)</param>
	/// <param name="chars">the resulting character internals (without quotes around it) - can be the same object as token</param>
	/// <param name="err">the error message if there was an error</param>
	bool TryExtractStringChars(const std::string &token, std::string &chars, std::string &err);
}

#endif
