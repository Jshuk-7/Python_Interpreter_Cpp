#include <filesystem>
#include <iostream>
#include <fstream>
#include <variant>
#include <string>

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

class Lexer
{
public:
	Lexer(const std::string& filename, const std::string& source)
		: m_Filename(filename), m_Source(source) { }

	void TrimLeft()
	{
		while (CursorActive() && isspace(m_Source[m_Cursor]))
			ShiftRight();
	}

	bool CursorActive() const { return m_Cursor < m_Source.size(); }
	bool CursorAtEnd() const { return !CursorActive(); }

	CursorPosition GetCursorPos() const
	{
		return { m_Filename, m_Row, m_Cursor - m_LineStart };
	}

	void ShiftRight()
	{
		if (CursorAtEnd())
			return;

		m_Cursor++;

		if (m_Source[m_Cursor] == '\n')
		{
			m_LineStart = m_Cursor;
			m_Row++;
		}
	}

	void DropLine()
	{
		while (m_Source[m_Cursor] != '\n')
			ShiftRight();
		
		if (m_Source[m_Cursor] == '\n')
			ShiftRight();
	}

	Token NextToken()
	{
		TrimLeft();

		while (m_Source[m_Cursor] == '#')
		{
			DropLine();
		}

		char first = m_Source[m_Cursor];
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

			while (m_Source[m_Cursor] != '"')
				ShiftRight();

			if (m_Source[m_Cursor] != '"')
			{
				COMPILER_ERROR("Error: %s, expected end of string literal", position.Display().c_str());
			}
			else
			{
				ShiftRight();
				std::string value = m_Source.substr(start, m_Cursor - start);
				return Token(TokenType::String, value, position);
			}
		}
		else if (isdigit(first))
		{
			ShiftRight();

			while (isdigit(m_Source[m_Cursor]))
				ShiftRight();

			int32_t value = std::atoi(m_Source.substr(start, m_Cursor - start).c_str());
			return Token(TokenType::Number, value, position);
		}
		else if (isalnum(first))
		{
			ShiftRight();

			while (isalnum(m_Source[m_Cursor]))
				ShiftRight();

			std::string value = m_Source.substr(start, m_Cursor - start);
			return Token(TokenType::Name, value, position);
		}

		return {};
	}

private:
	std::string m_Source = "";
	std::string m_Filename = "";

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
	stream.seekg(0, std::ios::beg);
	stream.read(contents.data(), size);

	std::string filename = std::filesystem::path(filepath).filename().string();

	Lexer lexer = Lexer(filename, contents.c_str());
	Token token = lexer.NextToken();

	while (token)
	{
		if (token.Type == TokenType::Name)
		{
			std::cout << token << '\n';
		}

		token = lexer.NextToken();
	}

	std::cin.get();
	return 0;
}