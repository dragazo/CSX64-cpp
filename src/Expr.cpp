#include "../include/Expr.h"
#include "../include/asm_common.h"

// -- Expr method impl  -- //

namespace CSX64::detail
{
	// this maps ops to a string representation - not used for parsing
	const std::unordered_map<Expr::OPs, std::string> Expr::Op_to_Str
	{
		{Expr::OPs::Mul, "*"},
		
		{Expr::OPs::SDiv, "/"},
		{Expr::OPs::SMod, "%"},

		{Expr::OPs::UDiv, "+/"},
		{Expr::OPs::UMod, "+%"},

		{Expr::OPs::Add, "+"},
		{Expr::OPs::Sub, "-"},

		{Expr::OPs::SHL, "<<"},
		{Expr::OPs::SHR, "+>>"},
		{Expr::OPs::SAR, ">>"},

		{Expr::OPs::SLess, "<"},
		{Expr::OPs::SLessE, "<="},
		{Expr::OPs::SGreat, ">"},
		{Expr::OPs::SGreatE, ">="},

		{Expr::OPs::SLess, "+<"},
		{Expr::OPs::SLessE, "+<="},
		{Expr::OPs::SGreat, "+>"},
		{Expr::OPs::SGreatE, "+>="},

		{Expr::OPs::Eq, "=="},
		{Expr::OPs::Neq, "!="},

		{Expr::OPs::BitAnd, "&"},
		{Expr::OPs::BitXor, "^"},
		{Expr::OPs::BitOr, "|"},

		{Expr::OPs::LogAnd, "&&"},
		{Expr::OPs::LogOr, "||"},

		{Expr::OPs::Neg, "-"},
		{Expr::OPs::BitNot, "~"},
		{Expr::OPs::LogNot, "!"},

		{Expr::OPs::Int, "$int"},
		{Expr::OPs::Float, "$float"},

		{Expr::OPs::Floor, "$floor"},
		{Expr::OPs::Ceil, "$ceil"},
		{Expr::OPs::Round, "$round"},
		{Expr::OPs::Trunc, "$trunc"},

		{Expr::OPs::Repr64, "$repr64"},
		{Expr::OPs::Repr32, "$repr32"},

		{Expr::OPs::Float64, "$float64"},
		{Expr::OPs::Float32, "$float32"},

		{Expr::OPs::Prec64, "$prec64"},
		{Expr::OPs::Prec32, "$prec32"},

		{Expr::OPs::Condition, "?"},
		{Expr::OPs::Pair, ":"},
	};

	// ------------------------------

	Expr::Expr() : _Result(0), _Floating(false), OP(OPs::None) {}

	Expr::Expr(const Expr &other) : _Token(other._Token), _Result(other._Result), _Floating(other._Floating), OP(other.OP)
	{
		// copy children recursively
		if (other.Left) Left = std::make_unique<Expr>(*other.Left);
		if (other.Right) Right = std::make_unique<Expr>(*other.Right);
	}
	Expr::Expr(Expr &&other) : _Token(std::move(other._Token)), _Result(other._Result), _Floating(other._Floating),
		OP(other.OP), Left(std::move(other.Left)), Right(std::move(other.Right))
	{
		// empty other
		other.OP = OPs::None;
		other._Token.clear();
	}

	Expr &Expr::operator=(const Expr &other)
	{
		// do it in terms of the copy ctor
		Expr temp(other);
		*this = std::move(temp);

		return *this;
	}
	Expr &Expr::operator=(Expr &&other)
	{
		using std::swap;

		swap(OP, other.OP);
		swap(Left, other.Left);
		swap(Right, other.Right);

		swap(_Result, other._Result);
		swap(_Floating, other._Floating);

		swap(_Token, other._Token);

		return *this;
	}

	void Expr::Clear()
	{
		OP = OPs::None;
		Left = nullptr;
		Right = nullptr;
	}

	void Expr::CacheResult(u64 result, bool floating)
	{
		// ensure this is now a leaf node
		OP = OPs::None;
		Left = nullptr;
		Right = nullptr;

		// discard token
		_Token.clear();

		// store data
		_Result = result;
		_Floating = floating;
	}

	Expr::Result Expr::_Evaluate(std::unordered_map<std::string, Expr> &symbols, std::string &err, std::vector<std::string> &visited)
	{
		u64 res = 0;
		bool floating = false;

		switch (OP)
		{
		case OPs::None: // -- value -- //
		{
			// if this has already been evaluated, return the cached result
			if (_Token.empty()) return { _Result, _Floating, Result::Type::Evaluated };

			// if it's a number
			if (std::isdigit(_Token[0]))
			{
				// remove underscores (e.g. 0b_0011_1101_1101_1111) and convert to lowercase for convenience
				std::string fixed_tok = to_lower(remove_char(_Token, '_'));
				std::string_view view = fixed_tok;
				
				// -- try parsing as int -- //

				// 0x is hex, 0o is oct, 0b is binary, otherwise decimal
				if (starts_with(view, "0x")) { if (TryParseUInt64(view.substr(2), res, 16)) break; }
				else if (starts_with(view, "0o")) { if (TryParseUInt64(view.substr(2), res, 8)) break; }
				else if (starts_with(view, "0b")) { if (TryParseUInt64(view.substr(2), res, 2)) break; }
				else if (TryParseUInt64(view, res, 10))
				{
					// to avoid confusion with C-style octal prefixes, leading zeros on decimal literals are invalid
					if (view.size() > 1 && view[0] == '0')
					{
						err = "decimal literals cannot have leading zeros (if octal was intended, use 0o prefix)";
						return { 0, false, Result::Type::Invalid };
					}
					break;
				}

				// -- try parsing as float -- //

				// try floating-point
				if (f64 fval; TryParseDouble(fixed_tok, fval)) { res = transmute<u64>(fval); floating = true; break; }

				// if nothing worked, it's an ill-formed numeric literal
				err = "Ill-formed numeric literal encountered: \"" + _Token + "\"";
				return { 0, false, Result::Type::Invalid };
			}
			// if it's a character constant
			else if (_Token[0] == '"' || _Token[0] == '\'' || _Token[0] == '`')
			{
				// get the characters
				std::string chars;
				if (!TryExtractStringChars(_Token, chars, err)) return { 0, false, Result::Type::Invalid };

				// must be 1-8 chars
				if (chars.size() == 0)
				{
					err = "Ill-formed character literal encountered (empty): " + _Token;
					return { 0, false, Result::Type::Invalid };
				}
				if (chars.size() > 8)
				{
					err = "Ill-formed character literal encountered (too long): " + _Token;
					return { 0, false, Result::Type::Invalid };
				}

				// build the value
				u64 res = 0;
				for (std::size_t i = 0; i < chars.size(); ++i) res |= (u64)(chars[i] & 0xff) << (i * 8);
			}
			// otherwise if it's a symbol we're already visited
			else if (contains(visited, _Token))
			{
				err = "Cyclic dependency on symbol \"" + _Token + "\"";
				return { 0, false, Result::Type::Invalid };
			}
			// otherwise if it's a defined symbol
			else if (Expr *expr; try_get_value(symbols, _Token, expr))
			{
				visited.push_back(_Token); // mark token as visited

				// if we can't evaluate it, fail
				Result r = expr->_Evaluate(symbols, err, visited);
				if (r.invalid()) return r;
				if (!r.evaluated())
				{
					err = "Failed to evaluate referenced symbol \"" + _Token + "\"\n-> " + err;
					return { 0, false, Result::Type::Incomplete };
				}

				res = r.val;
				floating = r.floating;

				visited.pop_back(); // unmark token (must be done for diamond expressions i.e. a=b+c, b=d, c=d, d=0)
			}
			// otherwise it must be a symbol that's not defined yet
			else
			{
				err = "Failed to evaluate \"" + _Token + "\"";
				return { 0, false, Result::Type::Incomplete };
			}

			break;
		}

		// -- binary operators -- //

		case OPs::Mul:
		{
			Result r1 = Left->_Evaluate(symbols, err, visited);
			if (r1.invalid()) return r1;
			Result r2 = Right->_Evaluate(symbols, err, visited);
			if (r2.invalid()) return r2;

			if (r1.incomplete() || r2.incomplete()) return { 0, false, Result::Type::Incomplete };

			if (r1.floating || r2.floating)
			{
				res = transmute<u64>((r1.floating ? transmute<f64>(r1.val) : (i64)r1.val) * (r2.floating ? transmute<f64>(r2.val) : (i64)r2.val));
				floating = true;
			}
			else res = r1.val * r2.val;
			
			break;
		}
		case OPs::SDiv:
		{
			Result r1 = Left->_Evaluate(symbols, err, visited);
			if (r1.invalid()) return r1;
			Result r2 = Right->_Evaluate(symbols, err, visited);
			if (r2.invalid()) return r2;

			if (r1.incomplete() || r2.incomplete()) return { 0, false, Result::Type::Incomplete };

			if (r1.floating || r2.floating)
			{
				f64 _num = r1.floating ? transmute<f64>(r1.val) : (f64)(i64)r1.val;
				f64 _denom = r2.floating ? transmute<f64>(r2.val) : (f64)(i64)r2.val;

				if (_denom == 0)
				{
					err = "divide by zero";
					return { 0, false, Result::Type::Invalid };
				}

				res = transmute<u64>(_num / _denom);
				floating = true;
			}
			else
			{
				if (r2.val == 0)
				{
					err = "divide by zero";
					return { 0, false, Result::Type::Invalid };
				}

				res = (u64)((i64)r1.val / (i64)r2.val);
			}

			break;
		}
		case OPs::SMod:
		{
			Result r1 = Left->_Evaluate(symbols, err, visited);
			if (r1.invalid()) return r1;
			Result r2 = Right->_Evaluate(symbols, err, visited);
			if (r2.invalid()) return r2;

			if (r1.incomplete() || r2.incomplete()) return { 0, false, Result::Type::Incomplete };

			if (r1.floating || r2.floating)
			{
				err = "attempt to modulo with floating point values";
				return { 0, false, Result::Type::Invalid };
			}
			if (r2.val == 0)
			{
				err = "divide by zero";
				return { 0, false, Result::Type::Invalid };
			}

			res = (u64)((i64)r1.val % (i64)r2.val);

			break;
		}
		case OPs::UDiv:
		{
			Result r1 = Left->_Evaluate(symbols, err, visited);
			if (r1.invalid()) return r1;
			Result r2 = Right->_Evaluate(symbols, err, visited);
			if (r2.invalid()) return r2;

			if (r1.incomplete() || r2.incomplete()) return { 0, false, Result::Type::Incomplete };

			if (r1.floating || r2.floating)
			{
				err = "attempt to unsigned divide with floating point values";
				return { 0, false, Result::Type::Invalid };
			}
			if (r2.val == 0)
			{
				err = "divide by zero";
				return { 0, false, Result::Type::Invalid };
			}

			res = r1.val / r2.val;

			break;
		}
		case OPs::UMod:
		{
			Result r1 = Left->_Evaluate(symbols, err, visited);
			if (r1.invalid()) return r1;
			Result r2 = Right->_Evaluate(symbols, err, visited);
			if (r2.invalid()) return r2;

			if (r1.incomplete() || r2.incomplete()) return { 0, false, Result::Type::Incomplete };

			if (r1.floating || r2.floating)
			{
				err = "attempt to unsigned modulo with floating point values";
				return { 0, false, Result::Type::Invalid };
			}
			if (r2.val == 0)
			{
				err = "divide by zero";
				return { 0, false, Result::Type::Invalid };
			}

			res = r1.val % r2.val;

			break;
		}
		case OPs::Add:
		{
			Result r1 = Left->_Evaluate(symbols, err, visited);
			if (r1.invalid()) return r1;
			Result r2 = Right->_Evaluate(symbols, err, visited);
			if (r2.invalid()) return r2;

			if (r1.incomplete() || r2.incomplete()) return { 0, false, Result::Type::Incomplete };

			if (r1.floating || r2.floating)
			{
				res = transmute<u64>((r1.floating ? transmute<f64>(r1.val) : (i64)r1.val) + (r2.floating ? transmute<f64>(r2.val) : (i64)r2.val));
				floating = true;
			}
			else res = r1.val + r2.val;

			break;
		}
		case OPs::Sub:
		{
			Result r1 = Left->_Evaluate(symbols, err, visited);
			if (r1.invalid()) return r1;
			Result r2 = Right->_Evaluate(symbols, err, visited);
			if (r2.invalid()) return r2;

			if (r1.incomplete() || r2.incomplete()) return { 0, false, Result::Type::Incomplete };

			if (r1.floating || r2.floating)
			{
				res = transmute<u64>((r1.floating ? transmute<f64>(r1.val) : (i64)r1.val) - (r2.floating ? transmute<f64>(r2.val) : (i64)r2.val));
				floating = true;
			}
			else res = r1.val - r2.val;

			break;
		}
		case OPs::SHL:
		{
			Result r1 = Left->_Evaluate(symbols, err, visited);
			if (r1.invalid()) return r1;
			Result r2 = Right->_Evaluate(symbols, err, visited);
			if (r2.invalid()) return r2;

			if (r1.incomplete() || r2.incomplete()) return { 0, false, Result::Type::Incomplete };
			if (r1.floating || r2.floating)
			{
				err = "attempt to bit shift floating point values";
				return { 0, false, Result::Type::Invalid };
			}

			// for size agnosticism, overshifting saturates as if infinite word size truncated to 64
			res = r2.val < 64 ? r1.val << r2.val : 0;

			break;
		}
		case OPs::SHR:
		{
			Result r1 = Left->_Evaluate(symbols, err, visited);
			if (r1.invalid()) return r1;
			Result r2 = Right->_Evaluate(symbols, err, visited);
			if (r2.invalid()) return r2;

			if (r1.incomplete() || r2.incomplete()) return { 0, false, Result::Type::Incomplete };
			if (r1.floating || r2.floating)
			{
				err = "attempt to bit shift floating point values";
				return { 0, false, Result::Type::Invalid };
			}

			// for size agnosticism, overshifting saturates as if infinite word size truncated to 64
			res = r2.val < 64 ? r1.val >> r2.val : 0;

			break;
		}
		case OPs::SAR:
		{
			Result r1 = Left->_Evaluate(symbols, err, visited);
			if (r1.invalid()) return r1;
			Result r2 = Right->_Evaluate(symbols, err, visited);
			if (r2.invalid()) return r2;

			if (r1.incomplete() || r2.incomplete()) return { 0, false, Result::Type::Incomplete };
			if (r1.floating || r2.floating)
			{
				err = "attempt to bit shift floating point values";
				return { 0, false, Result::Type::Invalid };
			}

			// for size agnosticism, overshifting saturates as if infinite word size truncated to 64
			if (r2.val < 64) res = (u64)((i64)r1.val >> r2.val);
			else res = r1.val & 0x8000000000000000u ? 0xffffffffffffffffu : 0u;

			break;
		}
		case OPs::SLess:
		{
			Result r1 = Left->_Evaluate(symbols, err, visited);
			if (r1.invalid()) return r1;
			Result r2 = Right->_Evaluate(symbols, err, visited);
			if (r2.invalid()) return r2;

			if (r1.incomplete() || r2.incomplete()) return { 0, false, Result::Type::Incomplete };

			if (r1.floating || r2.floating)
			{
				res = (r1.floating ? transmute<f64>(r1.val) : (i64)r1.val) < (r2.floating ? transmute<f64>(r2.val) : (i64)r2.val) ? 1 : 0ul;
			}
			else res = (i64)r1.val < (i64)r2.val ? 1 : 0ul;

			break;
		}
		case OPs::SLessE:
		{
			Result r1 = Left->_Evaluate(symbols, err, visited);
			if (r1.invalid()) return r1;
			Result r2 = Right->_Evaluate(symbols, err, visited);
			if (r2.invalid()) return r2;

			if (r1.incomplete() || r2.incomplete()) return { 0, false, Result::Type::Incomplete };

			if (r1.floating || r2.floating)
			{
				res = (r1.floating ? transmute<f64>(r1.val) : (i64)r1.val) <= (r2.floating ? transmute<f64>(r2.val) : (i64)r2.val) ? 1 : 0ul;
			}
			else res = (i64)r1.val <= (i64)r2.val ? 1 : 0ul;

			break;
		}
		case OPs::SGreat:
		{
			Result r1 = Left->_Evaluate(symbols, err, visited);
			if (r1.invalid()) return r1;
			Result r2 = Right->_Evaluate(symbols, err, visited);
			if (r2.invalid()) return r2;

			if (r1.incomplete() || r2.incomplete()) return { 0, false, Result::Type::Incomplete };

			if (r1.floating || r2.floating)
			{
				res = (r1.floating ? transmute<f64>(r1.val) : (i64)r1.val) > (r2.floating ? transmute<f64>(r2.val) : (i64)r2.val) ? 1 : 0ul;
			}
			else res = (i64)r1.val > (i64)r2.val ? 1 : 0ul;

			break;
		}
		case OPs::SGreatE:
		{
			Result r1 = Left->_Evaluate(symbols, err, visited);
			if (r1.invalid()) return r1;
			Result r2 = Right->_Evaluate(symbols, err, visited);
			if (r2.invalid()) return r2;

			if (r1.incomplete() || r2.incomplete()) return { 0, false, Result::Type::Incomplete };

			if (r1.floating || r2.floating)
			{
				res = (r1.floating ? transmute<f64>(r1.val) : (i64)r1.val) >= (r2.floating ? transmute<f64>(r2.val) : (i64)r2.val) ? 1 : 0ul;
			}
			else res = (i64)r1.val >= (i64)r2.val ? 1 : 0ul;

			break;
		}
		case OPs::ULess:
		{
			Result r1 = Left->_Evaluate(symbols, err, visited);
			if (r1.invalid()) return r1;
			Result r2 = Right->_Evaluate(symbols, err, visited);
			if (r2.invalid()) return r2;

			if (r1.incomplete() || r2.incomplete()) return { 0, false, Result::Type::Incomplete };
			if (r1.floating || r2.floating)
			{
				err = "attempt to perform unsigned comparison on floating point values";
				return { 0, false, Result::Type::Invalid };
			}

			res = r1.val < r2.val ? 1 : 0ul;

			break;
		}
		case OPs::ULessE:
		{
			Result r1 = Left->_Evaluate(symbols, err, visited);
			if (r1.invalid()) return r1;
			Result r2 = Right->_Evaluate(symbols, err, visited);
			if (r2.invalid()) return r2;

			if (r1.incomplete() || r2.incomplete()) return { 0, false, Result::Type::Incomplete };
			if (r1.floating || r2.floating)
			{
				err = "attempt to perform unsigned comparison on floating point values";
				return { 0, false, Result::Type::Invalid };
			}

			res = r1.val <= r2.val ? 1 : 0ul;

			break;
		}
		case OPs::UGreat:
		{
			Result r1 = Left->_Evaluate(symbols, err, visited);
			if (r1.invalid()) return r1;
			Result r2 = Right->_Evaluate(symbols, err, visited);
			if (r2.invalid()) return r2;

			if (r1.incomplete() || r2.incomplete()) return { 0, false, Result::Type::Incomplete };
			if (r1.floating || r2.floating)
			{
				err = "attempt to perform unsigned comparison on floating point values";
				return { 0, false, Result::Type::Invalid };
			}

			res = r1.val > r2.val ? 1 : 0ul;

			break;
		}
		case OPs::UGreatE:
		{
			Result r1 = Left->_Evaluate(symbols, err, visited);
			if (r1.invalid()) return r1;
			Result r2 = Right->_Evaluate(symbols, err, visited);
			if (r2.invalid()) return r2;

			if (r1.incomplete() || r2.incomplete()) return { 0, false, Result::Type::Incomplete };
			if (r1.floating || r2.floating)
			{
				err = "attempt to perform unsigned comparison on floating point values";
				return { 0, false, Result::Type::Invalid };
			}

			res = r1.val >= r2.val ? 1 : 0ul;

			break;
		}
		case OPs::Eq:
		{
			Result r1 = Left->_Evaluate(symbols, err, visited);
			if (r1.invalid()) return r1;
			Result r2 = Right->_Evaluate(symbols, err, visited);
			if (r2.invalid()) return r2;

			if (r1.incomplete() || r2.incomplete()) return { 0, false, Result::Type::Incomplete };

			if (r1.floating || r2.floating)
			{
				res = (r1.floating ? transmute<f64>(r1.val) : (i64)r1.val) == (r2.floating ? transmute<f64>(r2.val) : (i64)r2.val) ? 1 : 0ul;
			}
			else res = (i64)r1.val == (i64)r2.val ? 1 : 0ul;

			break;
		}
		case OPs::Neq:
		{
			Result r1 = Left->_Evaluate(symbols, err, visited);
			if (r1.invalid()) return r1;
			Result r2 = Right->_Evaluate(symbols, err, visited);
			if (r2.invalid()) return r2;

			if (r1.incomplete() || r2.incomplete()) return { 0, false, Result::Type::Incomplete };

			if (r1.floating || r2.floating)
			{
				res = (r1.floating ? transmute<f64>(r1.val) : (i64)r1.val) != (r2.floating ? transmute<f64>(r2.val) : (i64)r2.val) ? 1 : 0ul;
			}
			else res = (i64)r1.val != (i64)r2.val ? 1 : 0ul;

			break;
		}
		case OPs::BitAnd:
		{
			Result r1 = Left->_Evaluate(symbols, err, visited);
			if (r1.invalid()) return r1;
			Result r2 = Right->_Evaluate(symbols, err, visited);
			if (r2.invalid()) return r2;

			if (r1.incomplete() || r2.incomplete()) return { 0, false, Result::Type::Incomplete };
			if (r1.floating || r2.floating)
			{
				err = "attempt to perform bitwise on floating point values";
				return { 0, false, Result::Type::Invalid };
			}

			res = r1.val & r2.val;

			break;
		}
		case OPs::BitXor:
		{
			Result r1 = Left->_Evaluate(symbols, err, visited);
			if (r1.invalid()) return r1;
			Result r2 = Right->_Evaluate(symbols, err, visited);
			if (r2.invalid()) return r2;

			if (r1.incomplete() || r2.incomplete()) return { 0, false, Result::Type::Incomplete };
			if (r1.floating || r2.floating)
			{
				err = "attempt to perform bitwise on floating point values";
				return { 0, false, Result::Type::Invalid };
			}

			res = r1.val ^ r2.val;

			break;
		}
		case OPs::BitOr:
		{
			Result r1 = Left->_Evaluate(symbols, err, visited);
			if (r1.invalid()) return r1;
			Result r2 = Right->_Evaluate(symbols, err, visited);
			if (r2.invalid()) return r2;

			if (r1.incomplete() || r2.incomplete()) return { 0, false, Result::Type::Incomplete };
			if (r1.floating || r2.floating)
			{
				err = "attempt to perform bitwise on floating point values";
				return { 0, false, Result::Type::Invalid };
			}

			res = r1.val | r2.val;

			break;
		}
		case OPs::LogAnd:
		{
			Result r1 = Left->_Evaluate(symbols, err, visited);
			if (r1.invalid()) return r1;
			Result r2 = Right->_Evaluate(symbols, err, visited);
			if (r2.invalid()) return r2;

			if (r1.incomplete() || r2.incomplete()) return { 0, false, Result::Type::Incomplete };
			if (r1.floating || r2.floating)
			{
				err = "attempt to perform logical and on floating point values";
				return { 0, false, Result::Type::Invalid };
			}

			res = r1.val && r2.val ? 1 : 0ul;

			break;
		}
		case OPs::LogOr:
		{
			Result r1 = Left->_Evaluate(symbols, err, visited);
			if (r1.invalid()) return r1;
			Result r2 = Right->_Evaluate(symbols, err, visited);
			if (r2.invalid()) return r2;

			if (r1.incomplete() || r2.incomplete()) return { 0, false, Result::Type::Incomplete };
			if (r1.floating || r2.floating)
			{
				err = "attempt to perform logical or on floating point values";
				return { 0, false, Result::Type::Invalid };
			}

			res = r1.val || r2.val ? 1 : 0ul;

			break;
		}

		// -- unary ops -- //

		case OPs::Neg:
		{
			Result r = Left->_Evaluate(symbols, err, visited);
			if (!r.evaluated()) return r;

			res = r.floating ? transmute<u64>(-transmute<f64>(r.val)) : ~r.val + 1;
			floating = r.floating;

			break;
		}
		case OPs::BitNot:
		{
			Result r = Left->_Evaluate(symbols, err, visited);
			if (!r.evaluated()) return r;
			if (r.floating)
			{
				err = "attempt to perform bitwise not on floating point value";
				return { 0, false, Result::Type::Invalid };
			}

			res = ~r.val;

			break;
		}
		case OPs::LogNot:
		{
			Result r = Left->_Evaluate(symbols, err, visited);
			if (!r.evaluated()) return r;
			if (r.floating)
			{
				err = "attempt to perform logical not on floating point value";
				return { 0, false, Result::Type::Invalid };
			}

			res = r.val ? 0 : 1ul;

			break;
		}
		case OPs::Int:
		{
			Result r = Left->_Evaluate(symbols, err, visited);
			if (!r.evaluated()) return r;

			// float converts to int, otherwise pass-through
			res = r.floating ? (u64)(i64)transmute<f64>(r.val) : r.val;

			break;
		}
		case OPs::Float:
		{
			Result r = Left->_Evaluate(symbols, err, visited);
			if (!r.evaluated()) return r;

			res = r.floating ? r.val : transmute<u64>((f64)(i64)r.val);
			floating = true;

			break;
		}
		case OPs::Floor:
		{
			Result r = Left->_Evaluate(symbols, err, visited);
			if (!r.evaluated()) return r;

			res = r.floating ? transmute<u64>(std::floor(transmute<f64>(r.val))) : r.val;
			floating = r.floating;

			break;
		}
		case OPs::Ceil:
		{
			Result r = Left->_Evaluate(symbols, err, visited);
			if (!r.evaluated()) return r;

			res = r.floating ? transmute<u64>(std::ceil(transmute<f64>(r.val))) : r.val;
			floating = r.floating;

			break;
		}
		case OPs::Round:
		{
			Result r = Left->_Evaluate(symbols, err, visited);
			if (!r.evaluated()) return r;

			res = r.floating ? transmute<u64>(std::round(transmute<f64>(r.val))) : r.val;
			floating = r.floating;

			break;
		}
		case OPs::Trunc:
		{
			Result r = Left->_Evaluate(symbols, err, visited);
			if (!r.evaluated()) return r;

			res = r.floating ? transmute<u64>(std::trunc(transmute<f64>(r.val))) : r.val;
			floating = r.floating;

			break;
		}
		case OPs::Repr64:
		{
			Result r = Left->_Evaluate(symbols, err, visited);
			if (!r.evaluated()) return r;
			if (!r.floating)
			{
				err = "REPR64 requires a floating-point argument";
				return { 0, false, Result::Type::Invalid };
			}

			res = r.val;

			break;
		}
		case OPs::Repr32:
		{
			Result r = Left->_Evaluate(symbols, err, visited);
			if (!r.evaluated()) return r;
			if (!r.floating)
			{
				err = "REPR32 requires a floating-point argument";
				return { 0, false, Result::Type::Invalid };
			}

			res = transmute<u32>((f32)transmute<f64>(r.val));

			break;
		}
		case OPs::Float64:
		{
			Result r = Left->_Evaluate(symbols, err, visited);
			if (!r.evaluated()) return r;
			if (r.floating)
			{
				err = "FLOAT64 requires an integer argument";
				return { 0, false, Result::Type::Invalid };
			}

			res = r.val;
			floating = true;

			break;
		}
		case OPs::Float32:
		{
			Result r = Left->_Evaluate(symbols, err, visited);
			if (!r.evaluated()) return r;
			if (r.floating)
			{
				err = "FLOAT32 requires an integer argument";
				return { 0, false, Result::Type::Invalid };
			}
			if (r.val != (u32)r.val)
			{
				err = "argument to FLOAT32 was larger than 32-bit";
				return { 0, false, Result::Type::Invalid };
			}

			res = transmute<u64>((f64)transmute<f32>((u32)r.val));
			floating = true;

			break;
		}
		case OPs::Prec64:
		{
			Result r = Left->_Evaluate(symbols, err, visited);
			if (!r.evaluated()) return r;
			if (!r.floating)
			{
				err = "PREC64 requires a floating-point argument";
				return { 0, false, Result::Type::Invalid };
			}

			// truncate the precision to 64 bits (already storing in 64-bit floating point, so that would be no-op)
			res = r.val;
			floating = true;

			break;
		}
		case OPs::Prec32:
		{
			Result r = Left->_Evaluate(symbols, err, visited);
			if (!r.evaluated()) return r;
			if (!r.floating)
			{
				err = "PREC32 requires a floating-point argument";
				return { 0, false, Result::Type::Invalid };
			}

			// truncate the precision to 32 bits (stored as a 64-bit floating, just chop off some precision from the end and handle rounding)
			res = r.val & 0xffffffffe0000000u;
			if (r.val & 0x10000000u) res += 0x20000000u;
			floating = true;

			break;
		}

		// -- misc operators -- //

		case OPs::Condition:
		{
			Result cond = Left->_Evaluate(symbols, err, visited);
			if (cond.invalid()) return cond;
			Result r1 = Right->Left->_Evaluate(symbols, err, visited);
			if (r1.invalid()) return r1;
			Result r2 = Right->Right->_Evaluate(symbols, err, visited);
			if (r2.invalid()) return r2;

			if (cond.incomplete() || r1.incomplete() || r2.incomplete()) return { 0, false, Result::Type::Incomplete };
			if (cond.floating)
			{
				err = "attempt to use floating point value as conditon of ?: operator";
				return { 0, false, Result::Type::Invalid };
			}

			// because this is evaluated statically, we can allow branches to return different types
			if (cond.val)
			{
				res = r1.val;
				floating = r1.floating;
			}
			else
			{
				res = r2.val;
				floating = r2.floating;
			}

			break;
		}

		default: throw std::invalid_argument("Unknown operation in Expr");
		}

		CacheResult(res, floating);
		return { res, floating, Result::Type::Evaluated };
	}

	bool Expr::_FindPath(const std::string &value, std::vector<Expr*> &path, bool upper)
	{
		// mark ourselves as a candidate
		path.push_back(this);

		// if we're a leaf test ourself
		if (OP == OPs::None)
		{
			// if we found the value, we're done
			if ((upper ? to_upper(_Token) : _Token) == value) return true;
		}
		// otherwise test children
		else
		{
			// if they found it, we're done
			if (Left->_FindPath(value, path, upper) || (Right && Right->_FindPath(value, path, upper))) return true;
		}

		// otherwise we couldn't find it
		path.pop_back();
		return false;
	}

	void Expr::_GetStringValues(std::vector<std::string*> &vals)
	{
		// if we're a leaf
		if (OP == OPs::None)
		{
			// if we have a string value, add it
			if (!_Token.empty()) vals.push_back(&_Token);
		}
		// otherwise call on children
		else
		{
			Left->_GetStringValues(vals);
			if (Right) Right->_GetStringValues(vals);
		}
	}

	Expr::Result Expr::Evaluate(std::unordered_map<std::string, Expr> &symbols, std::string &err)
	{
		// refer to helper function
		std::vector<std::string> visited;
		return _Evaluate(symbols, err, visited);
	}

	bool Expr::FindPath(const std::string &value, std::vector<Expr*> &path, bool upper)
	{
		// make sure value isn't empty (would result in weird binding results since _Token is empty for null Token())
		if (value.empty()) throw std::invalid_argument("attempt to find empty string in expression tree");

		// refer to helper
		path.clear();
		return _FindPath(value, path, upper);
	}

	Expr *Expr::Find(const std::string &value, bool upper)
	{
		// if we're a leaf, test ourself
		if (OP == OPs::None) return (upper ? to_upper(_Token) : _Token) == value ? this : nullptr;
		// otherwise test children
		else
		{
			Expr *res = Left->Find(value, upper);
			return res ? res : Right ? Right->Find(value, upper) : nullptr;
		}
	}

	void Expr::Resolve(const std::string &expr, u64 result, bool floating)
	{
		// if we're a leaf
		if (OP == OPs::None)
		{
			// if we have this value, replace with result
			if (_Token == expr) CacheResult(result, floating);
		}
		// otherwise call on children
		else
		{
			Left->Resolve(expr, result, floating);
			if (Right) Right->Resolve(expr, result, floating);
		}
	}
	void Expr::Resolve(const std::string &expr, const std::string &value)
	{
		// if we're a leaf
		if (OP == OPs::None)
		{
			// if we have this value, replace with result
			if (_Token == expr) _Token = value;
		}
		// otherwise call on children
		else
		{
			Left->Resolve(expr, value);
			if (Right) Right->Resolve(expr, value);
		}
	}

	std::vector<std::string*> Expr::GetStringValues()
	{
		// call helper with an empty list
		std::vector<std::string*> vals;
		_GetStringValues(vals);
		return vals;
	}

	void Expr::PopulateAddSub(std::vector<Expr> &add, std::vector<Expr> &sub) &&
	{
		// if it's addition
		if (OP == Expr::OPs::Add)
		{
			// recurse to children
			std::move(*Left).PopulateAddSub(add, sub);
			std::move(*Right).PopulateAddSub(add, sub);
		}
		// if it's subtraction
		else if (OP == Expr::OPs::Sub)
		{
			// recurse to children
			std::move(*Left).PopulateAddSub(add, sub);
			std::move(*Right).PopulateAddSub(sub, add); // reverse add/sub lists to account for subtraction
		}
		// if it's negation
		else if (OP == Expr::OPs::Neg)
		{
			// recurse to child
			std::move(*Left).PopulateAddSub(sub, add); // reverse add/sub lists to account for subtraction
		}
		// otherwise it's not part of addition or subtraction
		else
		{
			// add to addition tree
			add.push_back(std::move(*this));
		}
	}

	Expr Expr::ChainAddition(const std::vector<Expr> &items)
	{
		Expr res;

		// if there's nothing, return a zero
		if (items.size() == 0) return res;
		// if there's 1 item, return it alone
		else if (items.size() == 1) { res = std::move(items[0]); return res; }
		// otherwise we have work to do
		else
		{
			// set up res for addition with the first item on the left
			res.OP = OPs::Add;
			res.Left = std::make_unique<Expr>(std::move(items[0]));

			// working position - we'll add new addition nodes to the right, with items on their left
			Expr *pos = &res;

			// for each additionl item except the last
			for (std::size_t i = 1; i < items.size() - 1; ++i)
			{
				// add this item to the tree
				pos->Right = std::make_unique<Expr>();
				pos = pos->Right.get();
				pos->OP = OPs::Add;
				pos->Left = std::make_unique<Expr>(std::move(items[i]));
			}

			// add the last item to the tree
			pos->Right = std::make_unique<Expr>(std::move(items.back()));

			// return the resulting tree
			return res;
		}
	}

	Expr Expr::CreateToken(std::string val)
	{
		Expr expr;
		expr.Token(std::move(val));
		return expr;
	}
	Expr Expr::CreateInt(u64 val)
	{
		Expr expr;
		expr.IntResult(val);
		return expr;
	}
	Expr Expr::CreateFloat(f64 val)
	{
		Expr expr;
		expr.FloatResult(val);
		return expr;
	}

	std::unique_ptr<Expr> Expr::NewToken(std::string val)
	{
		std::unique_ptr<Expr> expr = std::make_unique<Expr>();
		expr->Token(std::move(val));
		return expr;
	}
	std::unique_ptr<Expr> Expr::NewInt(u64 val)
	{
		std::unique_ptr<Expr> expr = std::make_unique<Expr>();
		expr->IntResult(val);
		return expr;
	}
	std::unique_ptr<Expr> Expr::NewFloat(f64 val)
	{
		std::unique_ptr<Expr> expr = std::make_unique<Expr>();
		expr->FloatResult(val);
		return expr;
	}

	std::ostream &Expr::WriteTo(std::ostream &writer, const Expr &expr)
	{
		// write type header
		write<u8>(writer, (u8)((!expr._Token.empty() ? 128 : 0) | (expr._Floating ? 64 : 0) | (expr.Right ? 32 : 0) | (int)expr.OP));

		// if it's a leaf
		if (expr.OP == OPs::None)
		{
			// if it's a token, write that
			if (!expr._Token.empty()) write_str(writer, expr._Token);
			// otherwise write the cached data
			else write<u64>(writer, expr._Result);
		}
		// otherwise it's an expression
		else
		{
			// do left branch
			WriteTo(writer, *expr.Left);
			// do right branch if non-null
			if (expr.Right) WriteTo(writer, *expr.Right);
		}

		return writer;
	}
	std::unique_ptr<Expr> Expr::_ReadFrom(std::istream &istr)
	{
		// create the node
		std::unique_ptr<Expr> expr = std::make_unique<Expr>();

		// read the type header
		u8 type;
		read<u8>(istr, type);

		// extract op
		expr->OP = (OPs)(type & 0x1f);

		// if it's a leaf
		if (expr->OP == OPs::None)
		{
			// if it's a token, read that
			if (type & 128) read_str(istr, expr->_Token);
			// otherwise read the cached data
			else
			{
				read<u64>(istr, expr->_Result);
				expr->_Floating = type & 64;
			}
		}
		// otherwise it's an expression
		else
		{
			// do left branch
			expr->Left = _ReadFrom(istr);
			// do right branch if non-null
			if (type & 32) expr->Right = _ReadFrom(istr);
		}

		return expr;
	}
	std::istream &Expr::ReadFrom(std::istream &reader, Expr &expr)
	{
		// read the expression
		std::unique_ptr<Expr> temp = _ReadFrom(reader);
		// move result to output
		expr = std::move(*temp);

		return reader;
	}

	std::ostream &operator<<(std::ostream &ostr, const Expr &expr)
	{
		if (expr.OP == Expr::OPs::None)
		{
			if (!expr._Token.empty()) ostr << expr._Token;
			else if (expr._Floating) ostr << transmute<f64>(expr._Result);
			else ostr << (i64)expr._Result;
		}
		else
		{
			// if we're a unary op
			if (expr.Right == nullptr)
			{
				ostr << Expr::Op_to_Str.at(expr.OP);

				ostr << '(';
				ostr << *expr.Left;
				ostr << ')';
			}
			// otherwise we're a binary op
			else
			{
				ostr << '(';
				ostr << *expr.Left;
				ostr << ')';

				ostr << Expr::Op_to_Str.at(expr.OP);

				ostr << '(';
				ostr << *expr.Right;
				ostr << ')';
			}
		}

		return ostr;
	}
}
