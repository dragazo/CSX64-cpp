#include <utility>
#include <vector>
#include <cstring>

#include "../include/Utility.h"
#include "../include/Assembly.h"

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
		for (u64 start = super.size() - sub.size(); start != npos; --start)
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

	// -----------------------------------------------------------------------------------

	std::ostream &BinaryLiteralCollection::write_to(std::ostream &writer) const
	{
		// write number of top level literals
		BinWrite<u64>(writer, top_level_literals.size());
		// then write each of them (length-prefixed)
		for (const auto &i : top_level_literals)
		{
			BinWrite<u64>(writer, i.size());
			BinWrite(writer, (const char*)i.data(), i.size());
		}

		// write number of literals
		BinWrite<u64>(writer, literals.size());
		// then write each of them
		for (const auto &i : literals)
		{
			BinWrite(writer, i.top_level_index);
			BinWrite(writer, i.start);
			BinWrite(writer, i.length);
		}

		return writer;
	}
	std::istream &BinaryLiteralCollection::read_from(std::istream &reader)
	{
		// discard current contents
		literals.clear();
		top_level_literals.clear();

		u64 temp, temp2;

		// read top level literals
		if (!BinRead(reader, temp)) return reader;
		top_level_literals.reserve(temp);
		for (u64 i = 0; i < temp; ++i)
		{
			if (!BinRead(reader, temp2)) return reader;
			std::vector<u8> v(temp2);
			if (!BinRead(reader, (char*)&v[0], temp2)) return reader;
			top_level_literals.emplace_back(std::move(v));
		}

		// read literals
		if (!BinRead(reader, temp)) return reader;
		literals.resize(temp);
		for (auto &i : literals)
		{
			if (!BinRead(reader, i.top_level_index) || !BinRead(reader, i.start) || !BinRead(reader, i.length)) return reader;
		}

		return reader;
	}
}
