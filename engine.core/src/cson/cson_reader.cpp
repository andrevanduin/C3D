
#include "cson_reader.h"

#include "asserts/asserts.h"
#include "platform/file_system.h"
#include "string/string_utils.h"

namespace C3D
{
    bool CSONReader::Read(const String& input, CSONObject& output)
    {
        // Take a pointer to the input string
        m_pInput = &input;

        // Tokenize the input string
        if (!Tokenize())
        {
            ERROR_LOG("Failed to tokenize input.");
            return false;
        }

        // Parse those tokens into CSONObjects
        if (!ParseTokens(output))
        {
            ERROR_LOG("Failed to parse tokens.");
            return false;
        }

        return true;
    }

    bool CSONReader::ReadFromFile(const String& path, CSONObject& output)
    {
        File file;
        if (!file.Open(path, FileModeRead))
        {
            ERROR_LOG("Failed to open CSON file: '{}'.", path);
            return false;
        }

        String input;
        if (!file.ReadAll(input))
        {
            ERROR_LOG("Failed to read CSON file: '{}'.", path);
            return false;
        }
        return Read(input, output);
    }

    bool CSONReader::TokenizeDefault(char c, u32& index, u32& line, CSONToken& outToken)
    {
        switch (c)
        {
            case ' ':
            case '\t':
                // Switch to parsing whitespace
                m_tokenizeMode = CSONTokenizeMode::Whitespace;
                outToken       = CSONToken(CSONTokenType::Whitespace, index, line);
                return true;
            case '#':
                // Switch to parsing comments
                m_tokenizeMode = CSONTokenizeMode::Comment;
                outToken       = CSONToken(CSONTokenType::Comment, index, line);
                return true;
            case '\n':
                // Increment our current line number
                line++;
                outToken = CSONToken(CSONTokenType::NewLine, index, line);
                return true;
            case '"':
                // Switch to parsing string literals
                m_tokenizeMode = CSONTokenizeMode::StringLiteral;
                outToken       = CSONToken(CSONTokenType::StringLiteral, index, line);
                return true;
            case '0':
            case '1':
            case '2':
            case '3':
            case '4':
            case '5':
            case '6':
            case '7':
            case '8':
            case '9':
                // Switch to parsing numeric literals
                m_tokenizeMode = CSONTokenizeMode::NumericLiteral;
                outToken       = CSONToken(CSONTokenType::Integer, index, line);
                return true;
            case '{':
                outToken = CSONToken(CSONTokenType::OpenCurlyBrace, index, line);
                return true;
            case '}':
                outToken = CSONToken(CSONTokenType::CloseCurlyBrace, index, line);
                return true;
            case ':':
                outToken = CSONToken(CSONTokenType::Colon, index, line);
                return true;
            case '[':
                outToken = CSONToken(CSONTokenType::OpenSquareBrace, index, line);
                return true;
            case ']':
                outToken = CSONToken(CSONTokenType::CloseSquareBrace, index, line);
                return true;
            case ',':
                outToken = CSONToken(CSONTokenType::Comma, index, line);
                return true;
            case '*':
                outToken = CSONToken(CSONTokenType::OperatorAsterisk, index, line);
                return true;
            case '+':
                outToken = CSONToken(CSONTokenType::OperatorPlus, index, line);
                return true;
            case '-':
                outToken = CSONToken(CSONTokenType::OperatorMinus, index, line);
                return true;
            case '/':
                outToken = CSONToken(CSONTokenType::OperatorSlash, index, line);
                return true;
            case 'f':
            case 'F':
                if (StringUtils::IEquals(m_pInput->Data() + index, "false", 5))
                {
                    outToken = CSONToken(CSONTokenType::Boolean, index, index + 4, line);
                    // Skip 4 characters for "false" because we will do another index++ after this
                    index += 4;
                    return true;
                }
                break;
            case 't':
            case 'T':
                if (StringUtils::IEquals(m_pInput->Data() + index, "true", 4))
                {
                    outToken = CSONToken(CSONTokenType::Boolean, index, index + 3, line);
                    // Skip 3 characters for "true" because we will do another index++ after this
                    index += 3;
                    return true;
                }
                break;
        }

        ERROR_LOG("Unsupported character found during tokenization: '{}'.", c);
        return false;
    }

    bool CSONReader::Tokenize()
    {
        // We start in default tokenize mode
        m_tokenizeMode = CSONTokenizeMode::Default;
        // Clear our token queue
        m_tokens.Clear();

        u32 index = 0;
        u32 line  = 1;

        while (index < m_pInput->Size())
        {
            auto c = (*m_pInput)[index];

            switch (m_tokenizeMode)
            {
                case CSONTokenizeMode::Default:
                {
                    CSONToken token;
                    if (TokenizeDefault(c, index, line, token))
                    {
                        m_tokens.Enqueue(token);
                    }
                    else
                    {
                        return false;
                    }
                }
                break;
                case CSONTokenizeMode::Comment:
                {
                    auto& token = m_tokens.PeekTail();
                    if (c != '\n')
                    {
                        // We have found another character in the comment so let's increment the end of the token
                        token.end++;
                    }
                    else
                    {
                        // We have found a newline which marks the end of this comment
                        m_tokenizeMode = CSONTokenizeMode::Default;
                        continue;
                    }
                }
                break;
                case CSONTokenizeMode::NumericLiteral:
                {
                    auto& token = m_tokens.PeekTail();
                    if (c >= '0' && c <= '9')
                    {
                        // We have found another numerical literal character so let's increment the end of token
                        token.end++;
                    }
                    else if (c == '.')
                    {
                        // We have found a dot, which means we are dealing with a double/float here
                        token.type = CSONTokenType::Float;
                        token.end++;
                    }
                    else if (c == 'e')
                    {
                        // We have found an e in the number, which means we are dealing with scientific notation
                        token.type = CSONTokenType::Scientific;
                        token.end++;
                    }
                    else if ((c == '-' || c == '+') && token.type == CSONTokenType::Scientific)
                    {
                        // Scientific notation uses + or -.
                        token.end++;
                    }
                    else
                    {
                        // No more numeric literals
                        m_tokenizeMode = CSONTokenizeMode::Default;
                        continue;
                    }
                }
                break;
                case CSONTokenizeMode::Whitespace:
                {
                    auto& token = m_tokens.PeekTail();
                    if (c == ' ')
                    {
                        // We have found more whitespace so let's increment the end of the token
                        token.end++;
                    }
                    else
                    {
                        // No more whitespace
                        m_tokenizeMode = CSONTokenizeMode::Default;
                        continue;
                    }
                }
                break;
                case CSONTokenizeMode::StringLiteral:
                {
                    auto& token = m_tokens.PeekTail();
                    token.end++;
                    if (c == '\"')
                    {
                        // We did found the closing '"' so we can stop parsing the string literal
                        m_tokenizeMode = CSONTokenizeMode::Default;
                    }
                }
                break;
            }

            index++;
        }

        // Make sure we end with a EndOfFile token to let the parser know when the file is at it's end
        m_tokens.Enqueue({ CSONTokenType::EndOfFile, index, index });

        return true;
    }

    bool CSONReader::ParseKeyOrEndOfObject(const CSONToken& token)
    {
        if (token.type == CSONTokenType::StringLiteral)
        {
            // Next token should be a colon
            m_parseMode = CSONParseMode::Colon;
            // Add a named property to the current object (skip the starting and ending '"')
            CSONProperty prop;
            prop.name = m_pInput->SubStr(token.start + 1, token.end);
            m_pCurrentObject->properties.EmplaceBack(prop);
            return true;
        }

        if (token.type == CSONTokenType::CloseCurlyBrace)
        {
            // We have found the end of the current object so check if we have a parent object
            if (m_pCurrentObject->parent)
            {
                // If we do we continue parsing for the parent
                m_pCurrentObject = m_pCurrentObject->parent;

                if (m_pCurrentObject->type == CSONObjectType::Array)
                {
                    // We are expecting a comma or the end of this array
                    m_parseMode = CSONParseMode::ArraySeparatorOrEnd;
                }
                else
                {
                    // We are expecting a comma or the end of this object
                    m_parseMode = CSONParseMode::CommaOrEndOfObject;
                }
            }
            else
            {
                // Otherwise we are done and we expect the end of the file
                m_parseMode = CSONParseMode::EndOfFile;
            }
            return true;
        }

        return ParseError(token, "string literal key or }");
    }

    bool CSONReader::ParseColon(const CSONToken& token)
    {
        if (token.type == CSONTokenType::Colon)
        {
            // Next up we should expect a value
            m_parseMode = CSONParseMode::Value;
            return true;
        }

        return ParseError(token, ":");
    }

    bool CSONReader::ParseValue(const CSONToken& token)
    {
        switch (token.type)
        {
            case CSONTokenType::Integer:
            {
                // Set the value to the last property (which we named in the ParseKey stage)
                m_pCurrentObject->properties.Last().value = m_pInput->SubStr(token.start, token.end + 1).ToI64();
                // Next up we expect a comma
                m_parseMode = CSONParseMode::CommaOrEndOfObject;
                return true;
            }
            case CSONTokenType::Float:
            case CSONTokenType::Scientific:
            {
                // Set the value to the last property (which we named in the ParseKey stage)
                m_pCurrentObject->properties.Last().value = m_pInput->SubStr(token.start, token.end + 1).ToF64();
                // Next up we expect a comma
                m_parseMode = CSONParseMode::CommaOrEndOfObject;
                return true;
            }
            case CSONTokenType::Boolean:
            {
                // Set the value to the last property (which we named in the ParseKey stage)
                m_pCurrentObject->properties.Last().value = m_pInput->SubStr(token.start, token.end + 1).ToBool();
                // Next up we expect a comma
                m_parseMode = CSONParseMode::CommaOrEndOfObject;
                return true;
            }
            case CSONTokenType::StringLiteral:
            {
                // Set the value to the last property (which we named in the ParseKey stage)
                m_pCurrentObject->properties.Last().value = m_pInput->SubStr(token.start + 1, token.end);
                // Next up we expect a comma
                m_parseMode = CSONParseMode::CommaOrEndOfObject;
                return true;
            }
            case CSONTokenType::OpenSquareBrace:
            {
                // The value provided appears to be an array so we create a new array object
                m_pCurrentObject->properties.Last().value = CSONArray(CSONObjectType::Array);
                // We take a pointer to the new array
                auto newCurrent = &std::get<CSONArray>(m_pCurrentObject->properties.Last().value);
                // Then we set it's parent to our current object
                newCurrent->parent = m_pCurrentObject;
                // Then we set the current object to the new array
                m_pCurrentObject = newCurrent;
                // Next up we expect array values (or for empty arrays a closing bracket)
                m_parseMode = CSONParseMode::ArrayValueAfterOpen;
                return true;
            }
            case CSONTokenType::OpenCurlyBrace:
            {
                // The value provided appears to be an object so we create a new object
                m_pCurrentObject->properties.Last().value = CSONObject(CSONObjectType::Object);
                // We take a pointer to the new object
                auto newCurrent = &std::get<CSONObject>(m_pCurrentObject->properties.Last().value);
                // Then we set it's parent to our current object
                newCurrent->parent = m_pCurrentObject;
                // Then we set the current object to the new object
                m_pCurrentObject = newCurrent;
                // Next up we expect a key inside of this object (or it's an empty object)
                m_parseMode = CSONParseMode::KeyOrEndOfObject;
                return true;
            }
            default:
                return ParseError(token, "a valid value");
        }
    }

    bool CSONReader::ParseCommaOrEndOfObject(const CSONToken& token)
    {
        if (token.type == CSONTokenType::Comma)
        {
            // We have found our comma so we should start parsing another key
            m_parseMode = CSONParseMode::KeyOrEndOfObject;
            return true;
        }

        if (token.type == CSONTokenType::CloseCurlyBrace)
        {
            // We have found the end of the object
            if (m_pCurrentObject->parent)
            {
                // We have a parent so we continue parsing that one
                m_pCurrentObject = m_pCurrentObject->parent;
                // Check if the parent was an object or an array
                if (m_pCurrentObject->type == CSONObjectType::Object)
                {
                    // We expect a comma or the end of that object next
                    m_parseMode = CSONParseMode::CommaOrEndOfObject;
                }
                else
                {
                    // We expect a comma or the end of the array next
                    m_parseMode = CSONParseMode::ArraySeparatorOrEnd;
                }
            }
            else
            {
                // No parent so we expect the end of the file
                m_parseMode = CSONParseMode::EndOfFile;
            }
            return true;
        }

        return ParseError(token, ",");
    }

    bool CSONReader::ParseArrayOrObject(const CSONToken& token)
    {
        if (token.type == CSONTokenType::OpenCurlyBrace)
        {
            // We are parsing an object
            m_pCurrentObject->type = CSONObjectType::Object;
            // and we expect a key next
            m_parseMode = CSONParseMode::KeyOrEndOfObject;
            return true;
        }

        if (token.type == CSONTokenType::OpenSquareBrace)
        {
            // We are parsing an array
            m_pCurrentObject->type = CSONObjectType::Array;
            // and expect array values next
            m_parseMode = CSONParseMode::ArrayValueAfterOpen;
            return true;
        }

        return ParseError(token, "{ or [");
    }

    bool CSONReader::ParseArrayValue(const CSONToken& token)
    {
        switch (token.type)
        {
            case CSONTokenType::OperatorMinus:
            {
                // We have found a negative sign so we expect a valid number after it
                m_parseMode = CSONParseMode::NegativeArrayValue;
                return true;
            }
            case CSONTokenType::Integer:
            {
                auto value = m_pInput->SubStr(token.start, token.end + 1).ToI64();
                m_pCurrentObject->properties.EmplaceBack(value);
                m_parseMode = CSONParseMode::ArraySeparatorOrEnd;
                return true;
            }
            case CSONTokenType::Float:
            case CSONTokenType::Scientific:
            {
                auto value = m_pInput->SubStr(token.start, token.end + 1).ToF64();
                m_pCurrentObject->properties.EmplaceBack(value);
                m_parseMode = CSONParseMode::ArraySeparatorOrEnd;
                return true;
            }
            case CSONTokenType::Boolean:
            {
                auto value = m_pInput->SubStr(token.start, token.end + 1).ToBool();
                m_pCurrentObject->properties.EmplaceBack(value);
                m_parseMode = CSONParseMode::ArraySeparatorOrEnd;
                return true;
            }
            case CSONTokenType::StringLiteral:
            {
                auto value = m_pInput->SubStr(token.start + 1, token.end);
                m_pCurrentObject->properties.EmplaceBack(value);
                m_parseMode = CSONParseMode::ArraySeparatorOrEnd;
                return true;
            }
            case CSONTokenType::OpenCurlyBrace:
            {
                // We have to add an object to our array
                auto obj = CSONObject(CSONObjectType::Object);
                // Set the parent of this object to our current (array)
                obj.parent = m_pCurrentObject;
                // Add the object to the array
                m_pCurrentObject->properties.EmplaceBack(obj);
                // Set the current object to the object we just added
                m_pCurrentObject = &std::get<CSONObject>(m_pCurrentObject->properties.Last().value);
                // Set the parse mode to start checking for items in the object
                m_parseMode = CSONParseMode::KeyOrEndOfObject;
                return true;
            }
            case CSONTokenType::CloseSquareBrace:
            {
                if (m_parseMode == CSONParseMode::ArrayValueAfterOpen)
                {
                    // Since we just parsed the array open it's ok to find the close bracket (since then it's just an empty array)
                    m_parseMode = CSONParseMode::CommaOrEndOfObject;
                    // We should also start parsing for the parent object again
                    m_pCurrentObject = m_pCurrentObject->parent;
                    return true;
                }
            }
        }

        return ParseError(token, "a valid value");
    }

    bool CSONReader::ParseNegativeArrayValue(const CSONToken& token)
    {
        switch (token.type)
        {
            case CSONTokenType::Integer:
            {
                // Start the string 1 token earlier to account for the minus sign
                auto value = m_pInput->SubStr(token.start - 1, token.end + 1).ToI64();
                m_pCurrentObject->properties.EmplaceBack(value);
                m_parseMode = CSONParseMode::ArraySeparatorOrEnd;
                return true;
            }
            case CSONTokenType::Float:
            case CSONTokenType::Scientific:
            {
                // Start the string 1 token earlier to account for the minus sign
                auto value = m_pInput->SubStr(token.start - 1, token.end + 1).ToF64();
                m_pCurrentObject->properties.EmplaceBack(value);
                m_parseMode = CSONParseMode::ArraySeparatorOrEnd;
                return true;
            }
        }

        return ParseError(token, "a valid number");
    }

    bool CSONReader::ParseArraySeparatorOrEnd(const CSONToken& token)
    {
        if (token.type == CSONTokenType::Comma)
        {
            // Separator found. So let's find another value
            m_parseMode = CSONParseMode::ArrayValueAfterComma;
            return true;
        }

        if (token.type == CSONTokenType::CloseSquareBrace)
        {
            // End of the array found. Next token should be a comma (or the end of the object)
            m_parseMode = CSONParseMode::CommaOrEndOfObject;
            // We should also start parsing for the parent object again
            m_pCurrentObject = m_pCurrentObject->parent;
            return true;
        }

        return ParseError(token, "',' or ']'");
    }

    bool CSONReader::ParseEndOfFile(const CSONToken& token)
    {
        if (token.type == CSONTokenType::EndOfFile)
        {
            // We are done
            return true;
        }

        return ParseError(token, "end of file");
    }

    bool CSONReader::ParseError(const CSONToken& token, const char* expected)
    {
        ERROR_LOG("Parsing error on line: {}. Expected: '{}' but found: '{}'.", token.line, expected, m_pInput->SubStr(token.start, token.end + 1));
        return false;
    }

    bool CSONReader::ParseTokens(CSONObject& output)
    {
        // Initially we always expect an object or an array
        m_parseMode = CSONParseMode::ObjectOrArray;

        // The root node that we should always have
        output = CSONObject(CSONObjectType::Object);

        // Get a pointer to our current object (which starts at the root)
        m_pCurrentObject = &output;

        auto token = m_tokens.Pop();
        while (token.type != CSONTokenType::EndOfFile)
        {
            // Skip comments and whitespace
            if (token.type == CSONTokenType::Whitespace || token.type == CSONTokenType::NewLine || token.type == CSONTokenType::Comment)
            {
                token = m_tokens.Pop();
                continue;
            }

            switch (m_parseMode)
            {
                case CSONParseMode::CommaOrEndOfObject:
                    if (!ParseCommaOrEndOfObject(token)) return false;
                    break;
                case CSONParseMode::Colon:
                    if (!ParseColon(token)) return false;
                    break;
                case CSONParseMode::KeyOrEndOfObject:
                    if (!ParseKeyOrEndOfObject(token)) return false;
                    break;
                case CSONParseMode::Value:
                    if (!ParseValue(token)) return false;
                    break;
                case CSONParseMode::ObjectOrArray:
                    if (!ParseArrayOrObject(token)) return false;
                    break;
                case CSONParseMode::ArrayValueAfterOpen:
                case CSONParseMode::ArrayValueAfterComma:
                    if (!ParseArrayValue(token)) return false;
                    break;
                case CSONParseMode::NegativeArrayValue:
                    if (!ParseNegativeArrayValue(token)) return false;
                    break;
                case CSONParseMode::ArraySeparatorOrEnd:
                    if (!ParseArraySeparatorOrEnd(token)) return false;
                    break;
                case CSONParseMode::EndOfFile:
                    if (!ParseEndOfFile(token)) return false;
                    break;
            }

            // Get the next token
            token = m_tokens.Pop();
        }

        return true;
    }

    void CSONReader::Destroy() { m_tokens.Destroy(); }

}  // namespace C3D
