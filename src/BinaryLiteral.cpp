#include <utility>
#include <vector>
#include <cstring>

#include "../include/Utility.h"
#include "../include/Assembly.h"
#include "../include/csx_exceptions.h"

using namespace CSX64::detail;

namespace CSX64
{
	static constexpr std::size_t npos = ~(std::size_t)0;

	// returns the index in super where the entire contents of sub can be found.
	// if such an index is not found, returns npos
	std::size_t find_subregion(const std::vector<u8> &super, const std::vector<u8> &sub)
	{
		// firstly, if super is smaller than sub, it's definitely not going to work
		if (super.size() < sub.size()) return npos;

		// for each possible starting position for this to work
		for (std::size_t start = super.size() - sub.size(); start != npos; --start)
		{
			// if this subregion contains the same data, return the start position (success)
			if (std::memcmp(super.data() + start, sub.data(), sub.size()) == 0) return start;
		}

		// if we get here it wasn't found
		return npos;
	}

	std::size_t BinaryLiteralCollection::_insert(const BinaryLiteral &info)
	{
		// if this literal already existed verbatim, we don't need a duplicate entry for it in literals
		for (std::size_t i = 0; i < literals.size(); ++i) if (literals[i] == info) return i;

		// otherwise we need to insert it as a new literal
		literals.push_back(info);
		return literals.size() - 1;
	}
	std::size_t BinaryLiteralCollection::add(std::vector<u8> &&value)
	{
		// look for any top level literal that value is a subregion of
		for (std::size_t i = 0; i < top_level_literals.size(); ++i)
		{
			// if we found one, we can just share it
			if (std::size_t start = find_subregion(top_level_literals[i], value); start != npos) return _insert({ i, start, value.size() });
		}

		// if that didn't work, look for any top level literal that is a subregion of value (i.e. the other way)
		for (std::size_t i = 0; i < top_level_literals.size(); ++i)
		{
			// if we found one, we can replace this top level literal
			std::size_t start = find_subregion(value, top_level_literals[i]);
			if (start != npos)
			{
				// replace the top level literal with value and update the starting position of any literals that referenced the top level literal we just replaced
				top_level_literals[i] = std::move(value);
				for (auto &lit : literals) if (lit.top_level_index == i) lit.start += start;

				// now we need to look through the top level literals again and see if any of them are contained in value (the new top level literal)
				for (std::size_t j = 0; j < top_level_literals.size(); )
				{
					if (j != i)
					{
						// if top level literal j is contained in value, we can remove j
						start = find_subregion(top_level_literals[i], top_level_literals[j]);
						if (start != npos)
						{
							// remove j from the top level literal list via the move pop idiom
							top_level_literals[j] = std::move(top_level_literals.back());
							top_level_literals.pop_back();

							// if i was the one we moved, repoint i to j (its new home)
							if (i == top_level_literals.size()) i = j;

							// update all the literals to reflect the change
							for (auto &lit : literals)
							{
								// if it referenced the deleted top level literal, repoint it to value and apply the start offset
								if (lit.top_level_index == j)
								{
									lit.top_level_index = i;
									lit.start += start;
								}
								// if it referenced the moved top level literal (the one that we used for move pop idiom), repoint it to j
								else if (lit.top_level_index == top_level_literals.size())
								{
									lit.top_level_index = j;
								}
							}
						}
						else ++j;
					}
					else ++j;
				}

				// add the literal
				return _insert({ i, 0, top_level_literals[i].size() });
			}
		}

		// if that also didn't work then we just have to add value as a new top level literal
		top_level_literals.emplace_back(std::move(value));
		return _insert({ top_level_literals.size() - 1, 0, top_level_literals.back().size() });
	}

	void BinaryLiteralCollection::clear() noexcept
	{
		literals.clear();
		top_level_literals.clear();
	}

	// -----------------------------------------------------------------------------------

	std::ostream &BinaryLiteralCollection::write_to(std::ostream &writer) const
	{
		// write number of top level literals
		write<u64>(writer, (u64)top_level_literals.size());
		// then write each of them (length-prefixed)
		for (const auto &i : top_level_literals)
		{
			write<u64>(writer, (u64)i.size());
			write_bin(writer, i.data(), i.size());
		}

		// write number of literals
		write<u64>(writer, (u64)literals.size());
		// then write each of them
		for (const auto &i : literals)
		{
			write<u64>(writer, (u64)i.top_level_index);
			write<u64>(writer, (u64)i.start);
			write<u64>(writer, (u64)i.length);
		}

		return writer;
	}
	std::istream &BinaryLiteralCollection::read_from(std::istream &reader)
	{
		// discard current contents
		clear();

		u64 temp, temp2;

		// read top level literals
		if (!read<u64>(reader, temp)) return reader;
		if constexpr (std::numeric_limits<std::size_t>::max() < std::numeric_limits<u64>::max()) { if (temp != (std::size_t)temp) throw MemoryAllocException("Binary literal too large"); }
		top_level_literals.reserve((std::size_t)temp);
		for (u64 i = 0; i < temp; ++i)
		{
			if (!read<u64>(reader, temp2)) return reader;
			if constexpr (std::numeric_limits<std::size_t>::max() < std::numeric_limits<u64>::max()) { if (temp2 != (std::size_t)temp2) throw MemoryAllocException("Binary literal too large"); }
			std::vector<u8> v((std::size_t)temp2);
			if (!read_bin(reader, v.data(), (std::size_t)temp2)) return reader;
			top_level_literals.emplace_back(std::move(v));
		}

		// read literals
		if (!read<u64>(reader, temp)) return reader;
		if constexpr (std::numeric_limits<std::size_t>::max() < std::numeric_limits<u64>::max()) { if (temp != (std::size_t)temp) throw MemoryAllocException("Binary literal too large"); }
		literals.resize((std::size_t)temp);
		for (auto &i : literals)
		{
			u64 a, b, c;
			if (!read<u64>(reader, a) || !read<u64>(reader, b) || !read<u64>(reader, c)) return reader;
			if constexpr (std::numeric_limits<std::size_t>::max() < std::numeric_limits<u64>::max())
			{
				if (a != (std::size_t)a || b != (std::size_t)b || c != (std::size_t)c) throw MemoryAllocException("Binary literal too large");
			}
			i.top_level_index = (std::size_t)a;
			i.start = (std::size_t)b;
			i.length = (std::size_t)c;
		}

		return reader;
	}
}
