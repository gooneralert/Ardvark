#include "pch.h"
#include "LuaPreprocess.h"

#include <stack>
#include <string>
#include <unordered_set>
#include <vector>

namespace Cheat {
namespace Features {
namespace LuaPreprocess {
namespace {

struct Tok {
	std::string text;
	int start;
	int end;
};

int EqLevel(const std::string& s, int p)
{
	int c = 0;
	while (p < static_cast<int>(s.size()) && s[p] == '=') { ++c; ++p; }
	return c;
}

// identifier/keyword tokens with byte positions; strings/comments skipped
std::vector<Tok> Tokenize(const std::string& src)
{
	std::vector<Tok> out;
	int i = 0;
	const int n = static_cast<int>(src.size());
	while (i < n)
	{
		const char c = src[i];
		if (c <= ' ') { ++i; continue; }

		// line / block comment
		if (c == '-' && i + 1 < n && src[i + 1] == '-')
		{
			i += 2;
			if (i < n && src[i] == '[')
			{
				const int eq = EqLevel(src, i + 1);
				if (i + 1 + eq < n && src[i + 1 + eq] == '[')
				{
					i += 2 + eq;
					while (i < n)
					{
						if (src[i] == ']')
						{
							const int e2 = EqLevel(src, i + 1);
							if (e2 == eq && i + 2 + eq <= n && src[i + 1 + eq] == ']')
							{ i += 2 + eq; break; }
						}
						++i;
					}
					continue;
				}
			}
			while (i < n && src[i] != '\n') ++i;
			continue;
		}

		// long string [[...]] or [=[...]=]
		if (c == '[')
		{
			const int eq = EqLevel(src, i + 1);
			if (i + 1 + eq < n && src[i + 1 + eq] == '[')
			{
				i += 2 + eq;
				while (i < n)
				{
					if (src[i] == ']')
					{
						const int e2 = EqLevel(src, i + 1);
						if (e2 == eq && i + 2 + eq <= n && src[i + 1 + eq] == ']')
						{ i += 2 + eq; break; }
					}
					++i;
				}
				continue;
			}
		}

		// short string
		if (c == '"' || c == '\'')
		{
			const char q = c;
			++i;
			while (i < n && src[i] != q) { if (src[i] == '\\') ++i; ++i; }
			if (i < n) ++i;
			continue;
		}

		// identifier / keyword
		if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '_')
		{
			const int s = i;
			while (i < n)
			{
				const char d = src[i];
				if ((d >= 'A' && d <= 'Z') || (d >= 'a' && d <= 'z')
					|| d == '_' || (d >= '0' && d <= '9'))
					++i;
				else
					break;
			}
			out.push_back({ src.substr(s, i - s), s, i });
			continue;
		}

		++i;
	}
	return out;
}

} // namespace

std::string Preprocess(const std::string& src)
{
	// Pass 1: find which loops contain 'continue'
	std::unordered_set<int> continued;
	std::vector<std::pair<bool, int>> stk;
	int seq = 0;
	bool expDo = false;
	for (const auto& t : Tokenize(src))
	{
		if (t.text == "for" || t.text == "while") { stk.emplace_back(true, ++seq); expDo = true; }
		else if (t.text == "repeat")                { stk.emplace_back(true, ++seq); expDo = false; }
		else if (t.text == "if" || t.text == "function") { stk.emplace_back(false, 0); expDo = false; }
		else if (t.text == "do")                    { if (!expDo) stk.emplace_back(false, 0); expDo = false; }
		else if (t.text == "then")                  { expDo = false; }
		else if (t.text == "end" || t.text == "until") { if (!stk.empty()) stk.pop_back(); }
		else if (t.text == "continue")
		{
			for (auto it = stk.rbegin(); it != stk.rend(); ++it)
			{
				if (it->first) { continued.insert(it->second); break; }
			}
		}
	}
	if (continued.empty())
		return src;

	// Pass 2: rewrite continue -> goto, inject label before the loop's end/until
	std::string sb;
	sb.reserve(src.size() + 64 * continued.size());
	std::vector<std::pair<bool, int>> stk2;
	int seq2 = 0;
	bool expDo2 = false;
	int prev = 0;
	for (const auto& t : Tokenize(src))
	{
		sb.append(src, prev, t.start - prev);
		prev = t.end;

		if (t.text == "for" || t.text == "while")
		{
			stk2.emplace_back(true, ++seq2); expDo2 = true;
			sb += t.text;
		}
		else if (t.text == "repeat")
		{
			stk2.emplace_back(true, ++seq2); expDo2 = false;
			sb += t.text;
		}
		else if (t.text == "if" || t.text == "function")
		{
			stk2.emplace_back(false, 0); expDo2 = false;
			sb += t.text;
		}
		else if (t.text == "do")
		{
			if (!expDo2) stk2.emplace_back(false, 0);
			expDo2 = false;
			sb += "do";
		}
		else if (t.text == "then")
		{
			expDo2 = false;
			sb += "then";
		}
		else if (t.text == "end")
		{
			std::pair<bool, int> f = stk2.empty() ? std::make_pair(false, 0) : stk2.back();
			if (!stk2.empty()) stk2.pop_back();
			if (f.first && continued.count(f.second))
				sb += " ::__cont_" + std::to_string(f.second) + "__:: ";
			sb += "end";
		}
		else if (t.text == "until")
		{
			std::pair<bool, int> f = stk2.empty() ? std::make_pair(false, 0) : stk2.back();
			if (!stk2.empty()) stk2.pop_back();
			if (f.first && continued.count(f.second))
				sb += " ::__cont_" + std::to_string(f.second) + "__:: ";
			sb += "until";
		}
		else if (t.text == "continue")
		{
			int id = 0;
			for (auto it = stk2.rbegin(); it != stk2.rend(); ++it)
			{
				if (it->first) { id = it->second; break; }
			}
			sb += (id > 0) ? ("goto __cont_" + std::to_string(id) + "__") : "do end --[[continue]]";
		}
		else
		{
			sb += t.text;
		}
	}
	if (prev < static_cast<int>(src.size()))
		sb.append(src, prev, src.size() - prev);
	return sb;
}

} // namespace LuaPreprocess
} // namespace Features
} // namespace Cheat