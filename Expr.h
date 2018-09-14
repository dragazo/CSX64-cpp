#ifndef CSX64_EXPR_H
#define CSX64_EXPR_H

#include <string>
#include <iostream>
#include <unordered_map>
#include <vector>
#include <memory>

#include "CoreTypes.h"
#include "Utility.h"

namespace CSX64
{
	// Represents an expression used to compute a value, with options for using a symbol table for lookup
	class Expr
	{
	public:
		enum class OPs
		{
			None,

			// binary ops

			Mul, Div, Mod,
			Add, Sub,

			SL, SR,

			Less, LessE, Great, GreatE,
			Eq, Neq,

			BitAnd, BitXor, BitOr,
			LogAnd, LogOr,

			// unary ops

			Neg, BitNot, LogNot, Int, Float,

			// special

			Condition, Pair,
			NullCoalesce
		};

		// maps expr op values to a human-readable form
		static const std::unordered_map<Expr::OPs, std::string> Op_to_Str;

	private:
		std::string _Token;

		u64 _Result;
		bool _Floating;

		// Caches the specified result - converts this node into an evaluated leaf
		void CacheResult(u64 result, bool floating);

		// helper function for Evaluate()
		bool __Evaluate__(std::unordered_map<std::string, Expr> &symbols, u64 &res, bool &floating, std::string &err, std::vector<std::string> &visited);

		// helper function for the FindPath() variants
		bool _FindPath(const std::string &value, std::vector<Expr*> &path, bool upper);

		// helper for GetStringValues()
		void _GetStringValues(std::vector<std::string*> &vals);

		// helper for ReadFrom()
		static std::unique_ptr<Expr> _ReadFrom(std::istream &istr);

	public:

		OPs OP;
		Expr *Left, *Right;

		// creates an expression with a value of integral zero
		Expr() noexcept;
		// destroys the expression tree recursively, leaving the object in an undefined state
		~Expr() noexcept;

		// creates an expression tree by copying another tree
		Expr(const Expr &other) noexcept;
		// creates an expression tree using the resources of another tree. the other tree is left in a valid but undefined evaluated state
		Expr(Expr &&other) noexcept;

		// assigns this tree a copy of another tree
		Expr &operator=(const Expr &other) noexcept;
		// assigns this tree the resources of another tree. the other tree is left in a valid but undefined state.
		Expr &operator=(Expr &&other) noexcept;

		// frees children recursively and sets this node to a valid but undefined evaluated state - effectively creates an empty node
		void Clear();

		// returns a pointer to the unevaluated token or null if there is none (i.e. not a leaf or already evaluated)
		const std::string *Token() const { return _Token.empty() ? nullptr : &_Token; }
		// assigns this node the specified token to be evaluated - may not be an empty string
		template<typename T>
		void Token(T &&val)
		{
			_Token = std::forward<T>(val);
			if (_Token.empty()) throw std::invalid_argument("Expr token cannot be empty string");

			OP = OPs::None;
			delete Left;
			delete Right;
			Left = Right = nullptr;
		}

		// Gets if this node is a leaf
		bool IsLeaf() const { return OP == OPs::None; }
		// Gets if this node has been evaluated
		bool IsEvaluated() const { return OP == OPs::None && Token() == nullptr; }

		// Assigns this expression to be an evaluated integer
		void IntResult(u64 val) { CacheResult(val, false); }
		// Assigns this expression to be an evaluated floating-point value
		void FloatResult(double val) { CacheResult(DoubleAsUInt64(val), true); }

		/// <summary>
		/// Attempts to evaluate the hole, returning true on success
		/// </summary>
		/// <param name="symbols">the symbols table to use for lookup</param>
		/// <param name="res">the resulting value upon success</param>
		/// <param name="floating">flag denoting result is floating-point</param>
		/// <param name="err">error emitted upon failure</param>
		bool Evaluate(std::unordered_map<std::string, Expr> &symbols, u64 &res, bool &floating, std::string &err);
		/// <summary>
		/// Returns true if the expression is evaluatable
		/// </summary>
		/// <param name="symbols">the symbols table for lookup</param>
		bool Evaluatable(std::unordered_map<std::string, Expr> &symbols);

		/// <summary>
		/// Finds the path to the specified value in the expression tree. Returns true on success. This version reuses the stack object by first clearing its contents.
		/// </summary>
		/// <param name="value">the value to find</param>
		/// <param name="path">the path to the specified value, with the root at the bottom of the stack and the found node at the top</param>
		/// <param name="upper">true if the token should be converted to upper case before the comparison</param>
		bool FindPath(const std::string &value, std::vector<Expr*> &path, bool upper = false);

		/// <summary>
		/// Finds the value in the specified expression tree. Returns it on success, otherwise null
		/// </summary>
		/// <param name="value">the found node or null</param>
		/// <param name="upper">true if the token should be converted to upper case before the comparison</param>
		Expr *Find(const std::string &value, bool upper = false);

		/// <summary>
		/// Resolves all occurrences of (expr) with the specified value
		/// </summary>
		/// <param name="expr">the expression to be replaced</param>
		/// <param name="result">the value to resolve to</param>
		/// <param name="floating">marks if the value if floating point</param>
		void Resolve(const std::string &expr, u64 result, bool floating);
		/// <summary>
		/// Resolves all occurrences of (expr) with the specified value
		/// </summary>
		/// <param name="expr">the expression to be replaced</param>
		/// <param name="value">The value to replace it with</param>
		void Resolve(const std::string &expr, const std::string &value);

		// Gets a list of all the unevaluated string values in this expression
		std::vector<std::string*> GetStringValues();

		/// <summary>
		/// Populates add and sub lists with terms that are strictly being added and subtracted. All items in add are being added. All items in sub are subtracted.
		/// </summary>
		/// <param name="add">the resulting added terms. should be empty before the call</param>
		/// <param name="sub">the resulting subtracted terms. should be empty before the call</param>
		void PopulateAddSub(std::vector<Expr*> &add, std::vector<Expr*> &sub);
		void PopulateAddSub(std::vector<const Expr*> &add, std::vector<const Expr*> &sub) const;

		/*
		void _ToString(std::string &b)
		{
		if (OP == OPs::None)
		{
		b.Append(Token == null ? _Floating ? AsDouble(_Result).ToString("e17") : ((i64)_Result).ToString() : Token);
		}
		else
		{
		// if we're a unary op
		if (Right == nullptr)
		{
		b.Append(OP.ToString());

		b.Append('(');
		Left._ToString(b);
		b.Append(')');
		}
		// otherwise we're a binary op
		else
		{
		b.Append('(');
		Left._ToString(b);
		b.Append(')');

		b.Append(OP.ToString());

		b.Append('(');
		Right._ToString(b);
		b.Append(')');
		}
		}
		}
		std::string ToString() const
		{
		StringBuilder b = new StringBuilder();
		_ToString(b);
		return b.ToString();
		}
		*/

		// ----------------------------

		/// <summary>
		/// Creates an expression tree that adds all the items.
		/// Nodes are added to the tree via move construction - their values in the source will be changed
		/// </summary>
		/// <param name="items">the items to create a tree from</param>
		static Expr ChainAddition(const std::vector<const Expr*> &items);

		static Expr CreateToken(std::string val);
		static Expr CreateInt(u64 val);
		static Expr CreateFloat(double val);

		static std::unique_ptr<Expr> NewToken(std::string val);
		static std::unique_ptr<Expr> NewInt(u64 val);
		static std::unique_ptr<Expr> NewFloat(double val);

		// Writes a binary representation of an expression to the stream
		static std::ostream &WriteTo(std::ostream &writer, const Expr &expr);
		// Reads a binary representation of an expresion from the stream - expr is first cleared before use
		static std::istream &ReadFrom(std::istream &reader, Expr &expr);

		friend inline void swap(Expr &a, Expr &b) { a = std::move(b); }

		// --------------------------------

		// writes a human-readable form of the given expression. use WriteTo() for the binary form.
		friend std::ostream &operator<<(std::ostream &ostr, const Expr &expr);
	};
}

#endif
