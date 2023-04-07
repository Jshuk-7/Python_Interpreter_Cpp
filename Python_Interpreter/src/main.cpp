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
	Plus,
	Minus,
	Multiply,
	Divide,
	Equals,
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
			case 1:
				return stream << token.Position << ' ' << std::get<std::string>(token.Value);
		}

		ASSERT(false, "Invalid token!\n");
		return stream;
	}
};

Token print(const std::vector<Token>& args)
{
	for (const auto& arg : args)
	{
		switch (arg.Value.index())
		{
			case 0:
				std::cout << std::get<int32_t>(arg.Value);
				break;
			case 1:
				std::cout << std::get<std::string>(arg.Value);
				break;
		}
	}
	std::cout << '\n';

	return {};
}

Token typeof(const std::vector<Token>& args)
{
	std::string typeName = "";
	switch (args[0].Value.index())
	{
		case 0: typeName = "int32_t";                 break;
		case 1: typeName = "std::basic_string<char>"; break;
	}

	return Token(TokenType::String, typeName, args[0].Position);
}

static std::unordered_map<std::string, std::function<Token(const std::vector<Token>&)>> s_FuncMap =
{
	{ "print", print },
	{ "typeof", typeof },
};

class FuncDef
{
public:
	FuncDef() = default;
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
		while (CursorActive() && Current() != '\n')
			ShiftRight();
		
		if (Current() == '\n')
			ShiftRight();
	}

	Token VisitNode(Token startingTok, const CursorPosition& position)
	{
		uint32_t i = 1;

		while (CursorActive() && std::isspace(m_Source[m_Cursor + i]))
		{
			i++;
		}

		if (CursorAtEnd())
			return startingTok;

		const std::string operators = "+-*/=";
		char potentialOp = m_Source[m_Cursor + i];

		if (size_t opIndex = operators.find(potentialOp); opIndex != std::string::npos)
		{
			ShiftRight();

			switch (startingTok.Value.index())
			{
				case 0:
				{
					switch (opIndex)
					{
						case 0:
						{
							int32_t lhs = std::get<int32_t>(startingTok.Value);
							Token op = NextToken();
							TrimLeft();
							ShiftRight();
							Token nextTok = NextToken();
							int32_t rhs = std::get<int32_t>(nextTok.Value);
							int32_t result = lhs + rhs;
							return Token(TokenType::Number, result, position);
						}
						case 1:
						{
							int32_t lhs = std::get<int32_t>(startingTok.Value);
							Token op = NextToken();
							TrimLeft();
							ShiftRight();
							Token nextTok = NextToken();
							int32_t rhs = std::get<int32_t>(nextTok.Value);
							int32_t result = lhs - rhs;
							return Token(TokenType::Number, result, position);
						}
						case 2:
						{
							int32_t lhs = std::get<int32_t>(startingTok.Value);
							Token op = NextToken();
							TrimLeft();
							ShiftRight();
							Token nextTok = NextToken();
							int32_t rhs = std::get<int32_t>(nextTok.Value);
							int32_t result = lhs * rhs;
							return Token(TokenType::Number, result, position);
						}
						case 3:
						{
							int32_t lhs = std::get<int32_t>(startingTok.Value);
							Token op = NextToken();
							TrimLeft();
							ShiftRight();
							Token nextTok = NextToken();
							int32_t rhs = std::get<int32_t>(nextTok.Value);
							ASSERT(rhs != 0, "Invalid denominator!");
							int32_t result = lhs / rhs;
							return Token(TokenType::Number, result, position);
						}
						case 4:
						{
							ASSERT(false, "int32_t->operator= not implemented yet");
							break;
						}
					}

					break;
				}
				case 1:
				{
					switch (opIndex)
					{
						case 0:
						{
							std::string lhs = std::get<std::string>(startingTok.Value);
							Token op = NextToken();
							TrimLeft();
							ShiftRight();
							Token nextTok = NextToken();
							std::string rhs = std::get<std::string>(nextTok.Value);
							std::string result = lhs + rhs;
							return Token(TokenType::String, result, position);
						}
						case 1:
						{
							break;
						}
						case 2:
						{
							break;
						}
						case 3:
						{
							break;
						}
						case 4:
						{
							ASSERT(false, "std::string->operator= not implemented yet");
							break;
						}
					}

					break;
				}
			}
		}

		return startingTok;
	}

	Token ParseToken()
	{
		if (CursorAtEnd())
			return {};

		char first = Current();
		CursorPosition position = GetCursorPos();
		const uint32_t start = m_Cursor;

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

			while (CursorActive() && Current() != '"')
				ShiftRight();

			if (CursorAtEnd() || Current() != '"')
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
				
				Token token = Token(TokenType::String, value, position);
				return VisitNode(token, position);
			}
		}
		else if (std::isdigit(first))
		{
			ShiftRight();

			while (CursorActive() && std::isdigit(Current()))
				ShiftRight();

			int32_t value = std::atoi(m_Source.substr(start, m_Cursor - start).c_str());
			Token token = Token(TokenType::Number, value, position);
			return VisitNode(token, position);
		}
		else if (std::isalnum(first))
		{
			ShiftRight();

			while (CursorActive() && std::isalnum(Current()))
				ShiftRight();

			std::string funName = m_Source.substr(start, m_Cursor - start);
			if (s_FuncMap.contains(funName))
			{
				std::vector<Token> args;
				Token oparen = NextToken();
				Token arg = VisitNode(NextToken(), position);
				bool first = true;
				
				while (CursorActive() && arg.Type != TokenType::CParen)
				{	
					args.push_back(arg);

					Token next = NextToken();
					if (!next)
						break;

					if (next.Type == TokenType::CParen)
						break;

					if (CursorAtEnd())
						break;

					arg = VisitNode(next, position);
				}

				FuncDef func(funName, args);
				Token returnValue = func.Execute();

				if (returnValue.Type != TokenType::None)
					return returnValue;
			}

			return Token(TokenType::Name, funName, position);
		}

		return {};
	}

	Token NextToken()
	{
		TrimLeft();

		while (CursorActive() && Current() == '#')
			DropLine();

		return ParseToken();
	}

private:
	const std::string& m_Source;
	std::string m_Filename = "";

	uint32_t m_Cursor = 0;
	uint32_t m_LineStart = 0;
	uint32_t m_Row = 0;
};

void PrintHelp()
{
	std::cout << "HELP:\n\n" << "Arguments:\n"
		<< "-r          open in repl mode\n"
		<< "<filepath>  run a specific python file\n";
}

int ReplMode()
{
	std::cout << R"(Python 3.11 [MSC v.1929 64 bit (AMD64)] on win32
Type "help", "copyright", "credits" or "license" for more information.)" << "\n";

	std::string line;
	std::cout << ">>> ";
	
	while (std::getline(std::cin, line))
	{
		if (line == "exit")
			return 0;

		Lexer lexer = Lexer("<stdin>", line);
		Token token = lexer.NextToken();

		std::variant<int32_t, std::string> value;
		while (token)
		{
			switch (token.Type)
			{
				case TokenType::Number:
					std::cout << std::get<int32_t>(token.Value) << '\n';
					break;
				default:
					std::cout << std::get<std::string>(token.Value) << '\n';
					break;
			}

			token = lexer.NextToken();
		}

		std::cout << ">>> ";
	}
}

int OpenFile(const std::string& filepath)
{
	if (!std::filesystem::exists(filepath))
	{
		std::cout << "Filepath doesn't exist!\n";
		return -1;
	}

	std::fstream stream(filepath);
	if (!stream.is_open())
	{
		std::cout << "Failed to open file: " << filepath << '\n';
		return -1;
	}

	stream.seekg(0, std::ios::end);
	size_t size = stream.tellg();
	if (size == 0)
	{
		std::cout << "File was empty!\n";
		return -1;
	}

	std::string contents = "";
	contents.resize(size);
	stream.seekg(0, std::ios::beg);
	stream.read(contents.data(), size);
	stream.close();

	std::string filename = std::filesystem::path(filepath).filename().string();

	Lexer lexer = Lexer(filename, contents);
	Token token = lexer.NextToken();

	while (token)
	{
		token = lexer.NextToken();
	}

	return 0;
}

int main(int argc, char* argv[])
{
	if (argc == 1)
	{
		PrintHelp();
		std::cin.get();
		return -1;
	}

	std::string arg = argv[1];

	if (arg == "-r")
	{
		return ReplMode();
	}
	else if (std::filesystem::exists(arg))
	{
		int exitCode = OpenFile(arg);
		std::cin.get();
		return exitCode;
	}

	return -1;
}