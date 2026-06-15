//
// Created by Orgest on 5/19/2026.
//

#include "TOMLParser.h"
#include <charconv>
#include "FileManager.h"

// ==========================================
// LEXER IMPLEMENTATION
// ==========================================

void TomlLexer::SkipWhitespaceAndComments() noexcept
{
    while (!IsEOF())
    {
        if (char c = *ptr; c == ' ' || c == '\t' || c == '\r' || c == '\n')
        {
            ptr++;
        }
        else if (c == '#')
        {
            while (!IsEOF() && *ptr != '\n')
            {
                ptr++;
            }
        }
        else
        {
            break;
        }
    }
}

TomlToken TomlLexer::NextToken() noexcept
{
    SkipWhitespaceAndComments();
    if (IsEOF()) return { TomlTokenType::EndOfFile, {} };

    const char current = Peek();

    // Structural Symbols
    if (current == '=') { const char* s = ptr; Consume(); return { TomlTokenType::Equal,        std::string_view(s, 1) }; }
    if (current == ',') { const char* s = ptr; Consume(); return { TomlTokenType::Comma,        std::string_view(s, 1) }; }
    if (current == '.') { const char* s = ptr; Consume(); return { TomlTokenType::Dot,          std::string_view(s, 1) }; }
    if (current == '[') { const char* s = ptr; Consume(); return { TomlTokenType::LeftBracket,  std::string_view(s, 1) }; }
    if (current == ']') { const char* s = ptr; Consume(); return { TomlTokenType::RightBracket, std::string_view(s, 1) }; }
    if (current == '{') { const char* s = ptr; Consume(); return { TomlTokenType::LeftBrace,    std::string_view(s, 1) }; }
    if (current == '}') { const char* s = ptr; Consume(); return { TomlTokenType::RightBrace,   std::string_view(s, 1) }; }

    // Strings (Basic & Literal strings)
    if (current == '"' || current == '\'')
    {
        char quoteType = Consume();
        const char* start = ptr;
        while (!IsEOF() && Peek() != quoteType && Peek() != '\n')
        {
            ptr++;
        }
        if (IsEOF() || Peek() == '\n') return { TomlTokenType::Error, "Unterminated string" };

        std::string_view strValue(start, ptr - start);
        Consume(); // Consume closing quote
        return { TomlTokenType::String, strValue };
    }

    // Numeric Literals (With support for 1.1.0 underscores)
    if ((current >= '0' && current <= '9') || current == '-' || current == '+')
    {
        const char* start = ptr;
        while (!IsEOF())
        {
            char c = Peek();
            if ((c >= '0' && c <= '9') || c == '.' || c == '-' || c == '+' || c == 'e' || c == 'E' || c == '_')
            {
                ptr++;
            }
            else
            {
                break;
            }
        }
        return { TomlTokenType::Number, std::string_view(start, ptr - start) };
    }

    // Bare Keys / Identifiers & Bools
    if ((current >= 'a' && current <= 'z') || (current >= 'A' && current <= 'Z') || current == '_')
    {
        const char* start = ptr;
        while (!IsEOF())
        {
            char c = Peek();
            if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '-')
            {
                ptr++;
            }
            else
            {
                break;
            }
        }
        std::string_view ident(start, ptr - start);
        if (ident == "true" || ident == "false") return { TomlTokenType::Bool, ident };
        return { TomlTokenType::Identifier, ident };
    }

    return { TomlTokenType::Error, std::string_view(ptr, 1) };
}

// ==========================================
// PARSER IMPLEMENTATION
// ==========================================

Result<TomlValue> TomlParser::ParseFile(const char* filepath)
{
    auto sizeResult = FileManager::GetFileSize(filepath);
    if (!sizeResult) return std::unexpected(sizeResult.error());

    std::string rawBuffer;
    rawBuffer.resize(sizeResult.value());

    auto readResult = FileManager::ReadText(filepath, Span(rawBuffer.data(), rawBuffer.size()));
    if (!readResult) return std::unexpected(readResult.error());

    return Parse(rawBuffer);
}

Result<TomlValue> TomlParser::ParseValue(TomlLexer& lexer, TomlToken& currentToken)
{
    if (currentToken.type == TomlTokenType::String)
    {
        return TomlValue{ .data = std::string(currentToken.value) };
    }

    if (currentToken.type == TomlTokenType::Bool)
    {
        return TomlValue{ .data = (currentToken.value == "true") };
    }

    if (currentToken.type == TomlTokenType::Number)
    {
        char cleanBuffer[64];
        size_t cleanIdx = 0;
        bool isFloat = false;

        for (char c : currentToken.value)
        {
            if (c == '_') continue;
            if (c == '.' || c == 'e' || c == 'E') isFloat = true;
            if (cleanIdx < 63) cleanBuffer[cleanIdx++] = c;
        }
        cleanBuffer[cleanIdx] = '\0';

        if (isFloat)
        {
            f64 value = 0.0;
            auto [p, ec] = std::from_chars(cleanBuffer, cleanBuffer + cleanIdx, value);
            if (ec == std::errc{} && p == cleanBuffer + cleanIdx) return TomlValue{ .data = value };
        }
        else
        {
            i32 value = 0;
            auto [p, ec] = std::from_chars(cleanBuffer, cleanBuffer + cleanIdx, value);
            if (ec == std::errc{} && p == cleanBuffer + cleanIdx) return TomlValue{ .data = value };
        }
        return std::unexpected(TomlInvalidSyntax);
    }

    // Inline Tables (e.g., pos = { x = 1.0, y = 2.0 })
    if (currentToken.type == TomlTokenType::LeftBrace)
    {
        TomlTable inlineTable;
        currentToken = lexer.NextToken(); // Consume '{'

        while (currentToken.type != TomlTokenType::RightBrace)
        {
            if (currentToken.type == TomlTokenType::EndOfFile) return std::unexpected(TomlInvalidSyntax);

            // Read Key Path
            Vector<std::string_view> inlineKeyPath;
            while (currentToken.type == TomlTokenType::Identifier || currentToken.type == TomlTokenType::String)
            {
                inlineKeyPath.push_back(currentToken.value);
                currentToken = lexer.NextToken();
                if (currentToken.type == TomlTokenType::Dot) currentToken = lexer.NextToken();
                else break;
            }

            if (currentToken.type != TomlTokenType::Equal) return std::unexpected(TomlInvalidSyntax);
            currentToken = lexer.NextToken(); // Consume '='

            auto valRes = ParseValue(lexer, currentToken);
            if (!valRes) return std::unexpected(valRes.error());

            if (auto insRes = InsertValueAtPath(inlineTable, inlineKeyPath, std::move(*valRes)); !insRes)
                return std::unexpected(insRes.error());

            currentToken = lexer.NextToken();
            if (currentToken.type == TomlTokenType::Comma)
            {
                currentToken = lexer.NextToken(); // Consume ','
            }
            else if (currentToken.type != TomlTokenType::RightBrace)
            {
                return std::unexpected(TomlInvalidSyntax);
            }
        }
        return TomlValue{ .data = std::move(inlineTable) };
    }

    // Uniform Arrays
    if (currentToken.type == TomlTokenType::LeftBracket)
    {
        TomlArray arr;
        size_t masterTypeIndex = 0;
        bool isFirstElement = true;

        currentToken = lexer.NextToken(); // Consume '['

        while (currentToken.type != TomlTokenType::RightBracket)
        {
            if (currentToken.type == TomlTokenType::EndOfFile) return std::unexpected(TomlInvalidSyntax);

            auto elementValue = ParseValue(lexer, currentToken);
            if (!elementValue) return std::unexpected(elementValue.error());

            // Enforce Uniform Arrays
            if (isFirstElement)
            {
                masterTypeIndex = elementValue->data.index();
                isFirstElement = false;
            }
            else if (elementValue->data.index() != masterTypeIndex)
            {
                return std::unexpected(TomlTypeMismatch);
            }

            arr.push_back(std::move(*elementValue));

            currentToken = lexer.NextToken();
            if (currentToken.type == TomlTokenType::Comma)
            {
                currentToken = lexer.NextToken(); // Consume ','
            }
            else if (currentToken.type != TomlTokenType::RightBracket)
            {
                return std::unexpected(TomlInvalidSyntax);
            }
        }
        return TomlValue{ .data = std::move(arr) };
    }

    return std::unexpected(TomlInvalidSyntax);
}

Result<TomlTable*> TomlParser::NavigateToTablePath(TomlTable& root, const Vector<std::string_view>& path)
{
    TomlTable* current = &root;
    for (const auto& part : path)
    {
        auto it = current->find(part);
        if (it == current->end())
        {
            // Allocate string *only* on insertion miss
            it = current->emplace(std::string(part), TomlValue{ .data = TomlTable{} }).first;
        }
        else if (!it->second.is<TomlTable>())
        {
            return std::unexpected(TomlInvalidSyntax);
        }
        current = &std::get<TomlTable>(it->second.data);
    }
    return current;
}

Result<void> TomlParser::InsertValueAtPath(TomlTable& activeTable, const Vector<std::string_view>& keyPath, TomlValue&& value)
{
    if (keyPath.empty()) return std::unexpected(TomlInvalidSyntax);

    TomlTable* current = &activeTable;
    for (size_t i = 0; i < keyPath.size() - 1; ++i)
    {
        std::string_view part = keyPath[i];
        auto it = current->find(part);
        if (it == current->end())
        {
            it = current->emplace(std::string(part), TomlValue{ .data = TomlTable{} }).first;
        }
        else if (!it->second.is<TomlTable>())
        {
            return std::unexpected(TomlInvalidSyntax);
        }
        current = &std::get<TomlTable>(it->second.data);
    }

    std::string_view finalKey = keyPath.back();
    if (current->contains(finalKey)) return std::unexpected(TomlDuplicateKey);

    (*current)[std::string(finalKey)] = std::move(value);
    return {};
}

Result<TomlValue> TomlParser::Parse(std::string_view content)
{
    TomlTable rootTable;
    TomlTable* activeTable = &rootTable;

    TomlLexer lexer(content);
    TomlToken token = lexer.NextToken();

    while (token.type != TomlTokenType::EndOfFile)
    {
        if (token.type == TomlTokenType::Error) return std::unexpected(TomlInvalidSyntax);

        // Section Headers: [render.pipeline.settings]
        if (token.type == TomlTokenType::LeftBracket)
        {
            token = lexer.NextToken(); // Consume '['
            Vector<std::string_view> sectionPath;

            while (token.type == TomlTokenType::Identifier || token.type == TomlTokenType::String)
            {
                sectionPath.push_back(token.value);
                token = lexer.NextToken();
                if (token.type == TomlTokenType::Dot)
                {
                    token = lexer.NextToken(); // Consume '.'
                }
                else break;
            }

            if (token.type != TomlTokenType::RightBracket) return std::unexpected(TomlInvalidHeader);

            auto targetTable = NavigateToTablePath(rootTable, sectionPath);
            if (!targetTable) return std::unexpected(targetTable.error());

            activeTable = targetTable.value();
            token = lexer.NextToken();
            continue;
        }

        // Standard Assignments: structural.dotted.key = value
        Vector<std::string_view> keyPath;
        while (token.type == TomlTokenType::Identifier || token.type == TomlTokenType::String)
        {
            keyPath.push_back(token.value);
            token = lexer.NextToken();
            if (token.type == TomlTokenType::Dot)
            {
                token = lexer.NextToken(); // Consume '.'
            }
            else break;
        }

        if (token.type != TomlTokenType::Equal) return std::unexpected(TomlInvalidSyntax);
        token = lexer.NextToken(); // Consume '='

        auto parsedValue = ParseValue(lexer, token);
        if (!parsedValue) return std::unexpected(parsedValue.error());

        if (auto insertRes = InsertValueAtPath(*activeTable, keyPath, std::move(*parsedValue)); !insertRes)
            return std::unexpected(insertRes.error());

        token = lexer.NextToken();
    }

    return TomlValue{ .data = std::move(rootTable) };
}