#include <unordered_map>
#include <filesystem>
#include <functional>
#include <iostream>
#include <fstream>
#include <variant>
#include <string>
#include <vector>

#define DEFERED_EXIT(code, ...) { printf(__VA_ARGS__); __debugbreak(); return (code); }

#ifdef NDEBUG
#define ASSERT(expr, ...)
#else
#define ASSERT(expr, ...) { if(!(expr)){ printf(__VA_ARGS__); __debugbreak(); } }
#endif

#define COMPILER_ERROR(...) { printf(__VA_ARGS__); }

static constexpr auto filepath = "hello.py";

enum class TokenType
{
	None = 0,
	Name,
	String,
	Number,
	OParen,
	CParen,
};

struct CursorPosition
{
	std::string Filename = "";
	uint32_t Row = 0;
	uint32_t Column = 0;
	
	std::string Display() const
	{
		return std::format("{}:{}:{}", Filename, Row + 1, Column + 1);
	}

	friend std::ostream& operator<<(std::ostream& stream, const CursorPosition& position)
	{
		return stream << position.Display();
	}
};

struct Token
{
	TokenType Type = TokenType::None;
	std::variant<int32_t, std::string> Value;
	CursorPosition Position;

	Token() = default;
	Token(TokenType type, int32_t value, const CursorPosition& position)
		: Type(type), Value(value), Position(position) { }

	Token(TokenType type, const std::string& value, const CursorPosition& position)
		: Type(type), Value(value), Position(position) { }

	operator bool() const { return Type != TokenType::None; }

	friend std::ostream& operator<<(std::ostream& stream, const Token& token)
	{
		if (!token)
			return stream;

		switch (token.Value.index())
		{
			case 0:
				return stream << token.Position << ' ' << std::get<int32_t>(token.Value);
			default:
				return stream << token.Position << ' ' << std::get<std::string>(token.Value);
		}

		ASSERT(false, "Invalid token!\n");
		return stream;
	}
};

Token print(const std::vector<Token>& args)
{
	std::cout << std::get<std::string>(args[0].Value) << '\n';
	return {};
}

static std::unordered_map<std::string, std::function<Token(const std::vector<Token>&)>> s_FuncMap =
{
	{ "print", print },
};

class FuncDef
{
public:
	FuncDef(const std::string& name, const std::vector<Token>& args)
		: m_Name(name), m_Args(args) { }

	Token Execute()
	{
		if (s_FuncMap.contains(m_Name))
			return s_FuncMap.at(m_Name)(m_Args);

		return {};
	}

private:
	std::string m_Name;
	std::vector<Token> m_Args;
};

class Lexer
{
public:
	Lexer(const std::string& filename, const std::string& source)
		: m_Filename(filename), m_Source(source) { }

	void TrimLeft()
	{
		while (CursorActive() && isspace(Current()))
			ShiftRight();
	}

	bool CursorActive() const { return m_Cursor < m_Source.size(); }
	bool CursorAtEnd() const { return !CursorActive(); }

	CursorPosition GetCursorPos() const { return { m_Filename, m_Row, m_Cursor - m_LineStart }; }

	char Current() const { return m_Source[m_Cursor]; }

	void ShiftRight()
	{
		if (CursorAtEnd())
			return;

		m_Cursor++;

		if (Current() == '\n')
		{
			m_LineStart = m_Cursor;
			m_Row++;
		}
	}

	void DropLine()
	{
		while (Current() != '\n')
			ShiftRight();
		
		if (Current() == '\n')
			ShiftRight();
	}

	Token ParseToken()
	{
		char first = Current();
		CursorPosition position = GetCursorPos();
		uint32_t start = m_Cursor;

		if (first == '(')
		{
			ShiftRight();
			return Token(TokenType::OParen, std::string(&first, 1), position);
		}
		else if (first == ')')
		{
			ShiftRight();
			return Token(TokenType::CParen, std::string(&first, 1), position);
		}
		else if (first == '"')
		{
			ShiftRight();

			while (Current() != '"')
				ShiftRight();

			if (Current() != '"')
			{
				COMPILER_ERROR("Error: %s, expected end of string literal", position.Display().c_str());
			}
			else
			{
				ShiftRight();
				std::string value = m_Source.substr(start, m_Cursor - start);

				// remove quotes
				value.erase(value.begin());
				value.erase(value.end() - 1);
				
				return Token(TokenType::String, value, position);
			}
		}
		else if (isdigit(first))
		{
			ShiftRight();

			while (isdigit(Current()))
				ShiftRight();

			int32_t value = std::atoi(m_Source.substr(start, m_Cursor - start).c_str());
			return Token(TokenType::Number, value, position);
		}
		else if (isalnum(first))
		{
			ShiftRight();

			while (isalnum(Current()))
				ShiftRight();

			std::string funName = m_Source.substr(start, m_Cursor - start);
			if (s_FuncMap.contains(funName))
			{
				std::vector<Token> args;
				Token oparen = ParseToken();
				Token arg = ParseToken();

				while (arg.Type != TokenType::CParen)
				{
					args.push_back(arg);
					arg = ParseToken();
				}

				delete m_LastFunc;
				m_LastFunc = new FuncDef(funName, args);
			}

			return Token(TokenType::Name, funName, position);
		}

		return {};
	}

	Token NextToken()
	{
		TrimLeft();

		while (Current() == '#')
			DropLine();

		return ParseToken();
	}

	FuncDef* NextFunc()
	{
		return m_LastFunc;
	}

private:
	const std::string& m_Source;
	std::string m_Filename = "";

	FuncDef* m_LastFunc = nullptr;

	uint32_t m_Cursor = 0;
	uint32_t m_LineStart = 0;
	uint32_t m_Row = 0;
};

int main()
{
	if (!std::filesystem::exists(filepath))
		DEFERED_EXIT(-1, "Filepath doesn't exist!\n");

	std::fstream stream(filepath);
	if (!stream.is_open())
		DEFERED_EXIT(-1, "Failed to open file: '%s'\n", filepath);

	stream.seekg(0, std::ios::end);
	size_t size = stream.tellg();
	if (size == 0)
		DEFERED_EXIT(-1, "File was empty!\n");

	std::string contents = "";
	contents.resize(size);
	stream.seekg(0, std::ios::beg);
	stream.read(contents.data(), size);

	std::string filename = std::filesystem::path(filepath).filename().string();

	Lexer lexer = Lexer(filename, contents);
	Token token = lexer.NextToken();

	while (token)
	{
		const bool hasIdent = token.Type == TokenType::Name;
		const bool isFunc = token.Type == TokenType::Number ? false : s_FuncMap.contains(std::get<std::string>(token.Value));

		if (hasIdent && isFunc)
		{
			FuncDef* func = lexer.NextFunc();
			Token returnValue = func->Execute();

			if (returnValue.Type != TokenType::None)
			{
				std::cout << returnValue << '\n';
			}
		}

		token = lexer.NextToken();
	}

	std::cin.get();
	return 0;
}