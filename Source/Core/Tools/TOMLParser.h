//
// Created by Orgest on 5/19/2026.
//

#pragma once
#include <map>
#include <stdexcept>
#include <string>
#include <variant>
#include <string_view>

#include "Vector.h"
#include "../PrimTypes.h"


enum class TomlTokenType : u8
{
    EndOfFile,
    Error,

    Identifier,  // Bare keys
    String,      // "quoted values" or 'literal values'
    Number,      // e.g., 1_000, 3.1415, -5e-2
    Bool,        // true, false

    Equal,       // =
    Comma,       // ,
    Dot,         // .
    LeftBracket, // [
    RightBracket,// ]
    LeftBrace,   // { (For Inline Tables)
    RightBrace   // }
};

struct TomlToken
{
    TomlTokenType type = TomlTokenType::EndOfFile;
    std::string_view value;
};

struct TomlLexer
{
    const char* ptr = nullptr;
    const char* end = nullptr;

    explicit TomlLexer(std::string_view content) noexcept
        : ptr(content.data()), end(content.data() + content.size()) {}

    [[nodiscard]] TomlToken NextToken() noexcept;

private:
    void SkipWhitespaceAndComments() noexcept;
    [[nodiscard]] constexpr bool IsEOF() const noexcept { return ptr >= end; }
    [[nodiscard]] constexpr char Peek() const noexcept { return IsEOF() ? '\0' : *ptr; }
    constexpr char Consume() noexcept { return IsEOF() ? '\0' : *ptr++; }
};

struct TomlValue;
using TomlTable = std::map<std::string, TomlValue, std::less<>>;
using TomlArray = Vector<TomlValue>;

struct TomlValue
{
    std::variant<
        std::nullptr_t,
        bool,
        i32,
        f64,
        std::string,
        TomlArray,
        TomlTable
    > data = nullptr;

    // Deduce this for dict lookups 
    auto&& operator[](this auto&& self, std::string_view key) {
        auto&& table = std::get<TomlTable>(std::forward<decltype(self)>(self).data);
        auto it = table.find(key);
        if (it == table.end()) {
            throw std::out_of_range("Key not found in TomlTable");
        }
        return it->second;
    }

    // For array index lookups
    auto&& operator[](this auto&& self, size_t index) {
        return std::get<TomlArray>(std::forward<decltype(self)>(self).data)[index];
    }

    template<typename T>
    auto&& as(this auto&& self) {
        return std::get<T>(std::forward<decltype(self)>(self).data);
    }

    template<typename T>
    bool is() const { return std::holds_alternative<T>(data); }
};

struct TomlParser
{
    [[nodiscard]] static Result<TomlValue> ParseFile(const char* filepath);
    [[nodiscard]] static Result<TomlValue> Parse(std::string_view content);

private:
    [[nodiscard]] static Result<TomlValue> ParseValue(TomlLexer& lexer, TomlToken& currentToken);
    [[nodiscard]] static Result<TomlTable*> NavigateToTablePath(TomlTable& root, const Vector<std::string_view>& path);
    [[nodiscard]] static Result<void> InsertValueAtPath(TomlTable& activeTable, const Vector<std::string_view>& keyPath, TomlValue&& value);
};