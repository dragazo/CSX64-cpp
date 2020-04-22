#include "../include/Expr.h"

// -- Expr method impl  -- //

namespace CSX64
{
	// this maps ops to a string representation - not used for parsing
	const std::unordered_map<Expr::OPs, std::string> Expr::Op_to_Str
	{
		{Expr::OPs::Mul, "*"},
		
		{Expr::OPs::UDiv, "/"},
		{Expr::OPs::UMod, "%"},

		{Expr::OPs::SDiv, "//"},
		{Expr::OPs::SMod, "%%"},

		{Expr::OPs::Add, "+"},
		{Expr::OPs::Sub, "-"},

		{Expr::OPs::SL, "<<"},
		{Expr::OPs::SR, ">>"},

		{Expr::OPs::Less, "<"},
		{Expr::OPs::LessE, "<="},
		{Expr::OPs::Great, ">"},
		{Expr::OPs::GreatE, ">="},
		{Expr::OPs::Eq, "=="},
		{Expr::OPs::Neq, "!="},

		{Expr::OPs::BitAnd, "&"},
		{Expr::OPs::BitOr, "|"},
		{Expr::OPs::BitXor, "^"},
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
		{Expr::OPs::NullCoalesce, "??"},
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

	bool Expr::_Evaluate(std::unordered_map<std::string, Expr> &symbols, u64 &res, bool &floating, std::string &err, std::vector<std::string> &visited)
	{
		Expr *expr;
		const std::string *tok;

		res = 0; // initialize out params
		floating = false;

		u64 L, R, Aux; // parsing locations for left and right subtrees
		bool LF, RF, AuxF;

		bool ret = true; // return value

		// switch through op
		switch (OP)
		{
		case OPs::None: // -- value -- //
			tok = Token();

			// if this has already been evaluated, return the cached result
			if (tok == nullptr) { res = _Result; floating = _Floating; return true; }

			// if it's a number
			if (std::isdigit((*tok)[0]))
			{
				// remove underscores (e.g. 0b_0011_1101_1101_1111) and convert to lowercase for convenience
				std::string fixed_tok = ToLower(remove_ch((*tok), '_'));

				// -- try parsing as int -- //

				// hex prefixes
				if (StartsWith(fixed_tok, "0x") || StartsWith(fixed_tok, "0h")) { if (TryParseUInt64(fixed_tok.substr(2), res, 16)) break; }
				// hex suffixes
				else if (fixed_tok.back() == 'x' || fixed_tok.back() == 'h') { if (TryParseUInt64(fixed_tok.substr(0, fixed_tok.size() - 1), res, 16)) break; }

				// dec prefixes
				else if (StartsWith(fixed_tok, "0d") || StartsWith(fixed_tok, "0t")) { if (TryParseUInt64(fixed_tok.substr(2), res, 10)) break; }
				// dec suffixes
				else if (fixed_tok.back() == 'd' || fixed_tok.back() == 't') { if (TryParseUInt64(fixed_tok.substr(0, fixed_tok.size() - 1), res, 10)) break; }

				// oct prefixes
				else if (StartsWith(fixed_tok, "0o") || StartsWith(fixed_tok, "0q")) { if (TryParseUInt64(fixed_tok.substr(2), res, 8)) break; }
				// oct suffixes
				else if (fixed_tok.back() == 'o' || fixed_tok.back() == 'q') { if (TryParseUInt64(fixed_tok.substr(0, fixed_tok.size() - 1), res, 8)) break; }

				// bin prefixes
				else if (StartsWith(fixed_tok, "0b") || StartsWith(fixed_tok, "0y")) { if (TryParseUInt64(fixed_tok.substr(2), res, 2)) break; }
				// bin suffixes
				else if (fixed_tok.back() == 'b' || fixed_tok.back() == 'y') { if (TryParseUInt64(fixed_tok.substr(0, fixed_tok.size() - 1), res, 2)) break; }

				// otherwise is dec
				else { if (TryParseUInt64(fixed_tok, res, 10)) break; }

				// -- try parsing as float -- //

				// try floating-point
				double fval;
				if (TryParseDouble(fixed_tok, fval)) { res = DoubleAsUInt64(fval); floating = true; break; }

				// if nothing worked, it's an ill-formed numeric literal
				err = "Ill-formed numeric literal encountered: \"" + (*Token()) + "\"";
				return false;
			}
			// if it's a character constant
			else if ((*tok)[0] == '"' || (*tok)[0] == '\'' || (*tok)[0] == '`')
			{
				// get the characters
				std::string chars;
				if (!TryExtractStringChars((*tok), chars, err)) return false;

				// must be 1-8 chars
				if (chars.size() == 0) { err = "Ill-formed character literal encountered (empty): " + (*tok); return false; }
				if (chars.size() > 8) { err = "Ill-formed character literal encountered (too long): " + (*tok); return false; }

				res = 0; // zero res just in case that's removed from the top of the function later on

				// build the value
				for (int i = 0; i < (int)chars.size(); ++i) res |= (u64)(chars[i] & 0xff) << (i * 8);

				break;
			}
			// if it's a defined symbol we haven't already visited
			else if (!Contains(visited, *tok) && TryGetValue(symbols, (*tok), expr))
			{
				visited.push_back((*tok)); // mark token as visited

				// if we can't evaluate it, fail
				if (!expr->_Evaluate(symbols, res, floating, err, visited)) { err = "Failed to evaluate referenced symbol \"" + (*tok) + "\"\n-> " + err; return false; }

				visited.pop_back(); // unmark token (must be done for diamond expressions i.e. a=b+c, b=d, c=d, d=0)

				break; // break so we can resolve the reference
			}
			// otherwise we can't evaluate it
			else { err = "Failed to evaluate \"" + (*tok) + "\""; return false; }

		// -- binary operators -- //

		case OPs::Mul:
			if (!Left->_Evaluate(symbols, L, LF, err, visited)) ret = false;
			if (!Right->_Evaluate(symbols, R, RF, err, visited)) ret = false;
			if (ret == false) return false;

			if (LF || RF) { res = DoubleAsUInt64((LF ? AsDouble(L) : (i64)L) * (RF ? AsDouble(R) : (i64)R)); floating = true; }
			else res = L * R;
			break;

		case OPs::UDiv:
			if (!Left->_Evaluate(symbols, L, LF, err, visited)) ret = false;
			if (!Right->_Evaluate(symbols, R, RF, err, visited)) ret = false;
			if (ret == false) return false;

			if (LF || RF)
			{
				double _num = LF ? AsDouble(L) : (double)L;
				double _denom = RF ? AsDouble(R) : (double)R;

				// catch division by zero
				if (_denom == 0) { err = "divide by zero"; return false; }

				res = DoubleAsUInt64(_num / _denom);
				floating = true;
			}
			else
			{
				// catch division by zero
				if (R == 0) { err = "divide by zero"; return false; }
				res = L / R;
			}

			break;
		case OPs::UMod:
			if (!Left->_Evaluate(symbols, L, LF, err, visited)) ret = false;
			if (!Right->_Evaluate(symbols, R, RF, err, visited)) ret = false;
			if (ret == false) return false;

			if (LF || RF)
			{
				double _num = LF ? AsDouble(L) : (double)L;
				double _denom = RF ? AsDouble(R) : (double)R;

				// catch division by zero
				if (_denom == 0) { err = "divide by zero"; return false; }

				res = DoubleAsUInt64(std::fmod(_num, _denom));
				floating = true;
			}
			else
			{
				// catch division by zero
				if (R == 0) { err = "divide by zero"; return false; }
				res = L % R;
			}
			break;

		case OPs::SDiv:
			if (!Left->_Evaluate(symbols, L, LF, err, visited)) ret = false;
			if (!Right->_Evaluate(symbols, R, RF, err, visited)) ret = false;
			if (ret == false) return false;

			if (LF || RF)
			{
				double _num = LF ? AsDouble(L) : (double)(i64)L;
				double _denom = RF ? AsDouble(R) : (double)(i64)R;

				// catch division by zero
				if (_denom == 0) { err = "divide by zero"; return false; }

				res = DoubleAsUInt64(_num / _denom);
				floating = true;
			}
			else
			{
				// catch division by zero
				if (R == 0) { err = "divide by zero"; return false; }

				res = (u64)((i64)L / (i64)R);
			}

			break;
		case OPs::SMod:
			if (!Left->_Evaluate(symbols, L, LF, err, visited)) ret = false;
			if (!Right->_Evaluate(symbols, R, RF, err, visited)) ret = false;
			if (ret == false) return false;

			if (LF || RF)
			{
				double _num = LF ? AsDouble(L) : (double)(i64)L;
				double _denom = RF ? AsDouble(R) : (double)(i64)R;

				// catch division by zero in floating case
				if (_denom == 0) { err = "divide by zero"; return false; }
				
				res = DoubleAsUInt64(std::fmod(_num, _denom));
				floating = true;
			}
			else
			{
				// catch division by zero in integral case
				if (R == 0) { err = "divide by zero"; return false; }
				res = (u64)((i64)L % (i64)R);
			}
			break;

		case OPs::Add:
			if (!Left->_Evaluate(symbols, L, LF, err, visited)) ret = false;
			if (!Right->_Evaluate(symbols, R, RF, err, visited)) ret = false;
			if (ret == false) return false;

			if (LF || RF) { res = DoubleAsUInt64((LF ? AsDouble(L) : (i64)L) + (RF ? AsDouble(R) : (i64)R)); floating = true; }
			else res = L + R;
			break;
		case OPs::Sub:
			if (!Left->_Evaluate(symbols, L, LF, err, visited)) ret = false;
			if (!Right->_Evaluate(symbols, R, RF, err, visited)) ret = false;
			if (ret == false) return false;

			if (LF || RF) { res = DoubleAsUInt64((LF ? AsDouble(L) : (i64)L) - (RF ? AsDouble(R) : (i64)R)); floating = true; }
			else res = L - R;
			break;

		case OPs::SL:
			if (!Left->_Evaluate(symbols, L, LF, err, visited)) ret = false;
			if (!Right->_Evaluate(symbols, R, RF, err, visited)) ret = false;
			if (ret == false) return false;
			
			res = L << R; floating = LF || RF;
			break;
		case OPs::SR:
			if (!Left->_Evaluate(symbols, L, LF, err, visited)) ret = false;
			if (!Right->_Evaluate(symbols, R, RF, err, visited)) ret = false;
			if (ret == false) return false;

			res = L >> R; floating = LF || RF;
			break;

		case OPs::Less:
			if (!Left->_Evaluate(symbols, L, LF, err, visited)) ret = false;
			if (!Right->_Evaluate(symbols, R, RF, err, visited)) ret = false;
			if (ret == false) return false;

			if (LF || RF) res = (LF ? AsDouble(L) : (i64)L) < (RF ? AsDouble(R) : (i64)R) ? 1 : 0ul;
			else res = (i64)L < (i64)R ? 1 : 0ul;
			break;
		case OPs::LessE:
			if (!Left->_Evaluate(symbols, L, LF, err, visited)) ret = false;
			if (!Right->_Evaluate(symbols, R, RF, err, visited)) ret = false;
			if (ret == false) return false;

			if (LF || RF) res = (LF ? AsDouble(L) : (i64)L) <= (RF ? AsDouble(R) : (i64)R) ? 1 : 0ul;
			else res = (i64)L <= (i64)R ? 1 : 0ul;
			break;
		case OPs::Great:
			if (!Left->_Evaluate(symbols, L, LF, err, visited)) ret = false;
			if (!Right->_Evaluate(symbols, R, RF, err, visited)) ret = false;
			if (ret == false) return false;

			if (LF || RF) res = (LF ? AsDouble(L) : (i64)L) > (RF ? AsDouble(R) : (i64)R) ? 1 : 0ul;
			else res = (i64)L > (i64)R ? 1 : 0ul;
			break;
		case OPs::GreatE:
			if (!Left->_Evaluate(symbols, L, LF, err, visited)) ret = false;
			if (!Right->_Evaluate(symbols, R, RF, err, visited)) ret = false;
			if (ret == false) return false;

			if (LF || RF) res = (LF ? AsDouble(L) : (i64)L) >= (RF ? AsDouble(R) : (i64)R) ? 1 : 0ul;
			else res = (i64)L >= (i64)R ? 1 : 0ul;
			break;

		case OPs::Eq:
			if (!Left->_Evaluate(symbols, L, LF, err, visited)) ret = false;
			if (!Right->_Evaluate(symbols, R, RF, err, visited)) ret = false;
			if (ret == false) return false;

			if (LF || RF) res = (LF ? AsDouble(L) : (i64)L) == (RF ? AsDouble(R) : (i64)R) ? 1 : 0ul;
			else res = L == R ? 1 : 0ul;
			break;
		case OPs::Neq:
			if (!Left->_Evaluate(symbols, L, LF, err, visited)) ret = false;
			if (!Right->_Evaluate(symbols, R, RF, err, visited)) ret = false;
			if (ret == false) return false;

			if (LF || RF) res = (LF ? AsDouble(L) : (i64)L) != (RF ? AsDouble(R) : (i64)R) ? 1 : 0ul;
			else res = L != R ? 1 : 0ul;
			break;

		case OPs::BitAnd:
			if (!Left->_Evaluate(symbols, L, LF, err, visited)) ret = false;
			if (!Right->_Evaluate(symbols, R, RF, err, visited)) ret = false;
			if (ret == false) return false;

			res = L & R; floating = LF || RF;
			break;
		case OPs::BitXor:
			if (!Left->_Evaluate(symbols, L, LF, err, visited)) ret = false;
			if (!Right->_Evaluate(symbols, R, RF, err, visited)) ret = false;
			if (ret == false) return false;

			res = L ^ R; floating = LF || RF;
			break;
		case OPs::BitOr:
			if (!Left->_Evaluate(symbols, L, LF, err, visited)) ret = false;
			if (!Right->_Evaluate(symbols, R, RF, err, visited)) ret = false;
			if (ret == false) return false;

			res = L | R; floating = LF || RF;
			break;

		case OPs::LogAnd:
			if (!Left->_Evaluate(symbols, L, LF, err, visited)) ret = false;
			if (!Right->_Evaluate(symbols, R, RF, err, visited)) ret = false;
			if (ret == false) return false;

			res = !IsZero(L, LF) && !IsZero(R, RF) ? 1 : 0ul;
			break;
		case OPs::LogOr:
			if (!Left->_Evaluate(symbols, L, LF, err, visited)) ret = false;
			if (!Right->_Evaluate(symbols, R, RF, err, visited)) ret = false;
			if (ret == false) return false;

			res = !IsZero(L, LF) || !IsZero(R, RF) ? 1 : 0ul;
			break;

			// unary ops

		case OPs::Neg:
			if (!Left->_Evaluate(symbols, L, LF, err, visited)) return false;

			res = LF ? DoubleAsUInt64(-AsDouble(L)) : ~L + 1; floating = LF;
			break;
		case OPs::BitNot:
			if (!Left->_Evaluate(symbols, L, LF, err, visited)) return false;

			res = ~L; floating = LF;
			break;
		case OPs::LogNot:
			if (!Left->_Evaluate(symbols, L, LF, err, visited)) return false;

			res = IsZero(L, LF) ? 1 : 0ul;
			break;

		case OPs::Int:
			if (!Left->_Evaluate(symbols, L, LF, err, visited)) return false;

			// float converts to int, otherwise pass-through
			res = LF ? (u64)(i64)AsDouble(L) : L;
			break;
		case OPs::Float:
			if (!Left->_Evaluate(symbols, L, LF, err, visited)) return false;

			// int converts to float, otherwise pass-through
			res = LF ? L : DoubleAsUInt64((double)(i64)L);
			floating = true;
			break;

		case OPs::Floor:
			if (!Left->_Evaluate(symbols, L, LF, err, visited)) return false;

			// floor the result - results in a float
			res = DoubleAsUInt64(std::floor(LF ? AsDouble(L) : (double)(i64)L));
			floating = true;
			break;
		case OPs::Ceil:
			if (!Left->_Evaluate(symbols, L, LF, err, visited)) return false;

			// ceil the result - results in a float
			res = DoubleAsUInt64(std::ceil(LF ? AsDouble(L) : (double)(i64)L));
			floating = true;
			break;
		case OPs::Round:
			if (!Left->_Evaluate(symbols, L, LF, err, visited)) return false;

			// round the result - results in a float
			res = DoubleAsUInt64(std::round(LF ? AsDouble(L) : (double)(i64)L));
			floating = true;
			break;
		case OPs::Trunc:
			if (!Left->_Evaluate(symbols, L, LF, err, visited)) return false;

			// trunc the result - results in a float
			res = DoubleAsUInt64(std::trunc(LF ? AsDouble(L) : (double)(i64)L));
			floating = true;
			break;

		case OPs::Repr64:
			if (!Left->_Evaluate(symbols, L, LF, err, visited)) return false;
			if (!LF) { err = "REPR64 requires a floating-point argument"; return false; }

			// convert the float to a 64-bit representation (already storing as 64-bit representation)
			res = L;
			break;
		case OPs::Repr32:
			if (!Left->_Evaluate(symbols, L, LF, err, visited)) return false;
			if (!LF) { err = "REPR32 requires a floating-point argument"; return false; }

			// convert the float to a 32-bit representation
			res = FloatAsUInt32((float)AsDouble(L));
			break;

		case OPs::Float64:
			if (!Left->_Evaluate(symbols, L, LF, err, visited)) return false;
			if (LF) { err = "FLOAT64 requires an integer argument"; return false; }

			// convert the 64-bit representation to a float
			res = L;
			floating = true;
			break;
		case OPs::Float32:
			if (!Left->_Evaluate(symbols, L, LF, err, visited)) return false;
			if (LF) { err = "FLOAT32 requires an integer argument"; return false; }

			// convert the 32-bit representation to a float
			res = DoubleAsUInt64((double)AsFloat((u32)L));
			floating = true;
			break;

		case OPs::Prec64:
			if (!Left->_Evaluate(symbols, L, LF, err, visited)) return false;
			if (!LF) { err = "PREC64 requires a floating-point argument"; return false; }

			// truncate the precision to 64 bits (already storing in 64-bit floating point, so that would be no-op)
			res = L;
			floating = true;
			break;
		case OPs::Prec32:
			if (!Left->_Evaluate(symbols, L, LF, err, visited)) return false;
			if (!LF) { err = "PREC32 requires a floating-point argument"; return false; }

			// truncate the precision to 32 bits (stored as a 64-bit floating, just chop off some precision from the end)
			res = L & (u64)0xffffffffe0000000;
			floating = true;
			break;

		// -- misc operators -- //

		case OPs::NullCoalesce:
			if (!Left->_Evaluate(symbols, L, LF, err, visited)) ret = false;
			if (!Right->_Evaluate(symbols, R, RF, err, visited)) ret = false;
			if (ret == false) return false;

			if (!IsZero(L, LF)) { res = L; floating = LF; }
			else /*          */ { res = R; floating = RF; }
			break;
		case OPs::Condition:
			if (!Left->_Evaluate(symbols, Aux, AuxF, err, visited)) ret = false;
			if (!Right->Left->_Evaluate(symbols, L, LF, err, visited)) ret = false;
			if (!Right->Right->_Evaluate(symbols, R, RF, err, visited)) ret = false;
			if (ret == false) return false;

			if (!IsZero(Aux, AuxF)) { res = L; floating = LF; }
			else /*              */ { res = R; floating = RF; }
			break;

		default: err = "Unknown operation"; return false;
		}

		// cache the result
		CacheResult(res, floating);

		return true;
	}

	bool Expr::_FindPath(const std::string &value, std::vector<Expr*> &path, bool upper)
	{
		// mark ourselves as a candidate
		path.push_back(this);

		// if we're a leaf test ourself
		if (OP == OPs::None)
		{
			// if we found the value, we're done
			if ((upper ? ToUpper(_Token) : _Token) == value) return true;
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

	bool Expr::Evaluate(std::unordered_map<std::string, Expr> &symbols, u64 &res, bool &floating, std::string &err)
	{
		// refer to helper function
		std::vector<std::string> visited;
		return _Evaluate(symbols, res, floating, err, visited);
	}
	bool Expr::Evaluatable(std::unordered_map<std::string, Expr> &symbols)
	{
		u64 res;
		bool floating;
		std::string err;
		return Evaluate(symbols, res, floating, err);
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
		if (OP == OPs::None) return (upper ? ToUpper(_Token) : _Token) == value ? this : nullptr;
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
	Expr Expr::CreateFloat(double val)
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
	std::unique_ptr<Expr> Expr::NewFloat(double val)
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
			if (!expr._Token.empty()) BinWrite(writer, expr._Token);
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
			if (type & 128) BinRead(istr, expr->_Token);
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
			else if (expr._Floating) ostr << AsDouble(expr._Result);
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
