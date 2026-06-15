//
// Created by Orgest on 5/18/2026.
//

#include "FileManager.h"
#include "JSONParser.h"

Result<JsonValue> JsonParser::ParseFile(const char* filepath)
{
    // Type matching allows direct error forwarding
    auto sizeResult = FileManager::GetFileSize(filepath);
    if (!sizeResult) return std::unexpected(sizeResult.error());

    std::string rawBuffer;
    rawBuffer.resize(sizeResult.value() + 1);

    auto readResult = FileManager::ReadText(filepath, Span(rawBuffer.data(), rawBuffer.size()));
    if (!readResult) return std::unexpected(readResult.error());

    return Parse(rawBuffer);
}

Result<JsonValue> JsonParser::Parse(std::string_view json)
{
    return ParseValue(json);
}

void JsonParser::SkipWhitespace(std::string_view& json)
{
    while (!json.empty() && std::isspace(static_cast<unsigned char>(json.front())))
    {
        json.remove_prefix(1);
    }
}

Result<std::string> JsonParser::ParseString(std::string_view& json)
{
    json.remove_prefix(1); // Skip the opening quote '"'
    size_t closeQuote = json.find('"');
    if (closeQuote == std::string_view::npos) return std::unexpected(OrgErrCode::JsonPrematureEOF);

    std::string str{json.substr(0, closeQuote)};
    json.remove_prefix(closeQuote + 1); // Advance past closing quote '"'
    return str;
}

Result<JsonValue> JsonParser::ParseValue(std::string_view& json)
{
    SkipWhitespace(json);
    if (json.empty()) return std::unexpected(JsonPrematureEOF);

    const char front = json.front();
    if (front == '{') return ParseObject(json);
    if (front == '[') return ParseArray(json);
    if (front == '"')
    {
        return ParseString(json).and_then([](std::string&& s) -> Result<JsonValue>
        {
            return JsonValue{.data = std::move(s)};
        });
    }

    size_t endToken = json.find_first_of(" \t\n\r,]};");
    if (endToken == std::string_view::npos) endToken = json.size();

    std::string_view token = json.substr(0, endToken);
    json.remove_prefix(endToken);

    if (token == "true") return JsonValue{.data = true};
    if (token == "false") return JsonValue{.data = false};
    if (token == "null") return JsonValue{.data = nullptr};

    // C++23 string_view method optimizations
    if (token.contains('.'))
    {
        return JsonValue{.data = std::stod(std::string(token))};
    }
    return JsonValue{.data = std::stoi(std::string(token))};
}

Result<JsonValue> JsonParser::ParseObject(std::string_view& json)
{
    json.remove_prefix(1); // Skip opening branch bracket '{'
    JsonObject obj;

    while (true)
    {
        SkipWhitespace(json);
        if (json.empty()) return std::unexpected(JsonPrematureEOF);
        if (json.front() == '}')
        {
            json.remove_prefix(1);
            break;
        }
        if (json.front() == ',')
        {
            json.remove_prefix(1);
            continue;
        }

        auto keyRes = ParseString(json);
        if (!keyRes) return std::unexpected(keyRes.error());

        SkipWhitespace(json);
        if (json.empty() || json.front() != ':') return std::unexpected(JsonUnexpectedToken);
        json.remove_prefix(1); // Skip the split identifier symbol ':'

        auto valRes = ParseValue(json);
        if (!valRes) return std::unexpected(valRes.error());

        obj[std::move(*keyRes)] = std::move(*valRes);
    }
    return JsonValue{.data = std::move(obj)};
}

Result<JsonValue> JsonParser::ParseArray(std::string_view& json)
{
    json.remove_prefix(1); // Skip opening index bracket '['
    JsonArray arr;

    while (true)
    {
        SkipWhitespace(json);
        if (json.empty()) return std::unexpected(OrgErrCode::JsonPrematureEOF);
        if (json.front() == ']')
        {
            json.remove_prefix(1);
            break;
        }
        if (json.front() == ',')
        {
            json.remove_prefix(1);
            continue;
        }

        auto valRes = ParseValue(json);
        if (!valRes) return std::unexpected(valRes.error());

        arr.push_back(std::move(*valRes));
    }
    return JsonValue{.data = std::move(arr)};
}
