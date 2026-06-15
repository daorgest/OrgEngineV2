//
// Created by Orgest on 5/18/2026.
//

#pragma once
#include <fstream>
#include <map>
#include <string>
#include <variant>

#include "Vector.h"
#include "../PrimTypes.h"

struct JsonValue;
using JsonObject = std::map<std::string, JsonValue>;
using JsonArray  = Vector<JsonValue>;

struct JsonValue
{
    std::variant<
        std::nullptr_t,
        bool,
        i32,
        f64,
        std::string,
        JsonArray,
        JsonObject
    > data = nullptr;

    auto&& operator[](this auto&& self, const std::string& key)
    {
        return std::get<JsonObject>(std::forward<decltype(self)>(self).data).at(key);
    }

    auto&& operator[](this auto&& self, size_t index)
    {
        return std::get<JsonArray>(std::forward<decltype(self)>(self).data)[index];
    }

    template <typename T>
    auto&& as(this auto&& self)
    {
        return std::get<T>(std::forward<decltype(self)>(self).data);
    }

    template <typename T>
    bool is() const { return std::holds_alternative<T>(data); }
};

struct JsonParser
{
    static Result<JsonValue> ParseFile(const char* filepath);
    static Result<JsonValue> Parse(std::string_view json);

private:
    static void SkipWhitespace(std::string_view& json);
    static Result<std::string> ParseString(std::string_view& json);
    static Result<JsonValue> ParseValue(std::string_view& json);
    static Result<JsonValue> ParseObject(std::string_view& json);
    static Result<JsonValue> ParseArray(std::string_view& json);
};