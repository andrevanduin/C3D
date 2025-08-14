
#include "cson_writer.h"

#include "platform/file_system.h"

namespace C3D
{
    void CSONWriter::WriteProperty(const CSONProperty& property, String& output, bool last, bool isInlineArray)
    {
        // First we add the name (if it's not empty)
        if (!property.name.Empty())
        {
            output += String::FromFormat("\"{}\": ", property.name);
        }

        // Then we must print the value
        switch (property.GetType())
        {
            case PropertyTypeI64:
                output += String::FromFormat("{}", property.GetI64());
                break;
            case PropertyTypeF64:
                output += String::FromFormat("{}", property.GetF64());
                break;
            case PropertyTypeBool:
                output += String::FromFormat("{}", property.GetBool() ? "true" : "false");
                break;
            case PropertyTypeString:
                output += String::FromFormat("\"{}\"", property.GetString());
                break;
            case PropertyTypeObject:
                const auto& obj = property.GetObject();
                if (obj.type == CSONObjectType::Array)
                {
                    WriteArray(obj, output);
                }
                else
                {
                    WriteObject(obj, output);
                }
                break;
        }

        // Add a comma if this is not the last property
        if (!last)
        {
            if (isInlineArray)
            {
                output += ", ";
            }
            else
            {
                output += ",";
            }
        }
    }

    void CSONWriter::WriteArray(const CSONArray& array, String& output)
    {
        bool isEmpty          = array.properties.Empty();
        bool isBasicTypeArray = !isEmpty && array.properties[0].IsBasicType();

        // Start the array with a square opening bracket
        output += "[";

        // Add a newline if there are items and they are non-basic
        if (!isEmpty && !isBasicTypeArray)
        {
            // For arrays we at least 1 element we increase identation
            m_indentation++;
            NextLine(output);
        }
        else
        {
            // Add a space for inline non-empty arrays
            if (!isEmpty) output += " ";
        }

        // Then we iterate all properties and add them to the output
        for (u32 i = 0; i < array.properties.Size(); ++i)
        {
            const auto isLast = i == array.properties.Size() - 1;

            const auto& prop = array.properties[i];
            WriteProperty(prop, output, isLast, isBasicTypeArray);

            if (isLast && !isBasicTypeArray)
            {
                // After all properties we decrease indentation
                m_indentation--;
                // After the last property we always do a newline
                NextLine(output);
            }
            else
            {
                // Not the last property
                if (!prop.IsBasicType())
                {
                    // For non-basic types we do a newline
                    NextLine(output);
                }
            }
        }

        // Finally after all properties we close the array
        if (!isEmpty && isBasicTypeArray) output += " ";
        output += "]";
    }

    void CSONWriter::WriteObject(const CSONObject& object, String& output)
    {
        // For an object we increase indentation
        m_indentation++;

        // Start the object with a curly opening bracket
        output += "{";
        NextLine(output);

        // Then we iterate all properties and add them to the output
        for (u32 i = 0; i < object.properties.Size(); ++i)
        {
            const auto isLast = i == object.properties.Size() - 1;
            const auto& prop  = object.properties[i];
            WriteProperty(prop, output, isLast, false);

            if (isLast)
            {
                // After all properties we decrease indentation
                m_indentation--;
            }

            NextLine(output);
        }

        // Finally after all properties we close the object
        output += "}";
    }

    void CSONWriter::NextLine(String& output)
    {
        // Add a newline
        output += "\n";
        // Add 4 spaces in-place of a tab
        output += String::Repeat(' ', m_indentation * 4);
    }

    void CSONWriter::Write(const CSONObject& object, String& output)
    {
        if (object.type == CSONObjectType::Array)
        {
            WriteArray(object, output);
        }
        else
        {
            WriteObject(object, output);
        }
    }

    bool CSONWriter::WriteToFile(const CSONObject& object, const String& path)
    {
        File file;
        if (!file.Open(path, FileModeWrite))
        {
            ERROR_LOG("Failed to open CSON file: '{}'.", path);
            return false;
        }

        String output;
        Write(object, output);

        if (!file.Write(output.Data(), output.Size()))
        {
            ERROR_LOG("Failed to write CSON string to file: '{}'.", path);
            return false;
        }

        return true;
    }
}  // namespace C3D