
#include "cson_types.h"

namespace C3D
{
    const static String empty = "";
    const static CSONObject emptyObject(CSONObjectType::Object);
    const static CSONArray emptyArray(CSONObjectType::Array);
    const static vec4 emptyVec = { 0, 0, 0, 0 };

    bool CSONObject::HasProperty(const String& name) const
    {
        for (auto& p : properties)
        {
            if (p.name == name)
            {
                return true;
            }
        }

        return false;
    }

    bool CSONObject::GetPropertyByName(const String& propertyName, CSONProperty& property) const
    {
        for (auto& p : properties)
        {
            if (p.name == propertyName)
            {
                property = p;
                return true;
            }
        }

        return false;
    }

    bool CSONObject::GetPropertyValueByName(const String& propertyName, u16& value) const
    {
        CSONProperty prop;
        if (!GetPropertyByName(propertyName, prop) || !prop.HoldsInteger())
        {
            return false;
        }
        value = prop.GetU16();
        return true;
    }

    bool CSONObject::GetPropertyValueByName(const String& propertyName, u32& value) const
    {
        CSONProperty prop;
        if (!GetPropertyByName(propertyName, prop) || !prop.HoldsInteger())
        {
            return false;
        }
        value = prop.GetU32();
        return true;
    }

    bool CSONObject::GetPropertyValueByName(const String& propertyName, u64& value) const
    {
        CSONProperty prop;
        if (!GetPropertyByName(propertyName, prop) || !prop.HoldsInteger())
        {
            return false;
        }
        value = prop.GetU64();
        return true;
    }

    bool CSONObject::GetPropertyValueByName(const String& propertyName, i16& value) const
    {
        CSONProperty prop;
        if (!GetPropertyByName(propertyName, prop) || !prop.HoldsInteger())
        {
            return false;
        }
        value = prop.GetI16();
        return true;
    }

    bool CSONObject::GetPropertyValueByName(const String& propertyName, i32& value) const
    {
        CSONProperty prop;
        if (!GetPropertyByName(propertyName, prop) || !prop.HoldsInteger())
        {
            return false;
        }
        value = prop.GetI32();
        return true;
    }

    bool CSONObject::GetPropertyValueByName(const String& propertyName, i64& value) const
    {
        CSONProperty prop;
        if (!GetPropertyByName(propertyName, prop) || !prop.HoldsInteger())
        {
            return false;
        }
        value = prop.GetI64();
        return true;
    }

    bool CSONObject::GetPropertyValueByName(const String& propertyName, f32& value) const
    {
        CSONProperty prop;
        if (!GetPropertyByName(propertyName, prop))
        {
            return false;
        }
        if (prop.HoldsInteger())
        {
            value = static_cast<f32>(prop.GetI64());
            return true;
        }
        if (prop.HoldsFloat())
        {
            value = static_cast<f32>(prop.GetF64());
            return true;
        }
        return false;
    }

    bool CSONObject::GetPropertyValueByName(const String& propertyName, f64& value) const
    {
        CSONProperty prop;
        if (!GetPropertyByName(propertyName, prop))
        {
            return false;
        }
        if (prop.HoldsInteger())
        {
            value = static_cast<f64>(prop.GetI64());
            return true;
        }
        if (prop.HoldsFloat())
        {
            value = prop.GetF64();
            return true;
        }
        return false;
    }

    bool CSONObject::GetPropertyValueByName(const String& propertyName, bool& value) const
    {
        CSONProperty prop;
        if (!GetPropertyByName(propertyName, prop) || !prop.HoldsBool())
        {
            return false;
        }
        value = prop.GetBool();
        return true;
    }

    bool CSONObject::GetPropertyValueByName(const String& propertyName, String& value) const
    {
        CSONProperty prop;
        if (!GetPropertyByName(propertyName, prop) || !prop.HoldsString())
        {
            return false;
        }
        value = prop.GetString();
        return true;
    }

    bool CSONObject::GetPropertyValueByName(const String& propertyName, DynamicArray<CSONProperty>& value) const
    {
        CSONProperty prop;
        if (!GetPropertyByName(propertyName, prop) || !prop.HoldsArray())
        {
            return false;
        }
        value = prop.GetArray().properties;
        return true;
    }

    bool CSONObject::GetPropertyValueByName(const String& propertyName, CSONObject& value) const
    {
        CSONProperty prop;
        if (!GetPropertyByName(propertyName, prop) || !prop.HoldsObject())
        {
            return false;
        }
        value = prop.GetObject();
        return true;
    }

    CSONProperty::CSONProperty(u32 num) : value(static_cast<i64>(num)) {}
    CSONProperty::CSONProperty(i32 num) : value(static_cast<i64>(num)) {}
    CSONProperty::CSONProperty(u64 num) : value(static_cast<i64>(num)) {}
    CSONProperty::CSONProperty(i64 num) : value(static_cast<i64>(num)) {}
    CSONProperty::CSONProperty(f32 num) : value(static_cast<f64>(num)) {}
    CSONProperty::CSONProperty(f64 num) : value(static_cast<f64>(num)) {}
    CSONProperty::CSONProperty(bool b) : value(b) {}
    CSONProperty::CSONProperty(const String& str) : value(str) {}
    CSONProperty::CSONProperty(const CSONObject& obj) : value(obj) {}

    CSONProperty::CSONProperty(const String& name, u32 num) : name(name), value(static_cast<i64>(num)) {}
    CSONProperty::CSONProperty(const String& name, i32 num) : name(name), value(static_cast<i64>(num)) {}
    CSONProperty::CSONProperty(const String& name, u64 num) : name(name), value(static_cast<i64>(num)) {}
    CSONProperty::CSONProperty(const String& name, i64 num) : name(name), value(static_cast<i64>(num)) {}
    CSONProperty::CSONProperty(const String& name, f32 num) : name(name), value(static_cast<f64>(num)) {}
    CSONProperty::CSONProperty(const String& name, f64 num) : name(name), value(static_cast<f64>(num)) {}
    CSONProperty::CSONProperty(const String& name, bool b) : name(name), value(b) {}
    CSONProperty::CSONProperty(const String& name, const String& str) : name(name), value(str) {}
    CSONProperty::CSONProperty(const String& name, const vec4& v) : name(name)
    {
        auto array = CSONArray(CSONObjectType::Array);
        array.properties.EmplaceBack(v.x);
        array.properties.EmplaceBack(v.y);
        array.properties.EmplaceBack(v.z);
        array.properties.EmplaceBack(v.w);
        value = array;
    }

    CSONProperty::CSONProperty(const String& name, const CSONObject& obj) : name(name), value(obj) {}

    u32 CSONProperty::GetType() const { return value.index(); }

    bool CSONProperty::IsBasicType() const { return value.index() == PropertyTypeBool || value.index() == PropertyTypeF64 || value.index() == PropertyTypeI64; }

    bool CSONProperty::GetBool() const { return std::get<bool>(value); }
    bool CSONProperty::HoldsBool() const { return std::holds_alternative<bool>(value); }

    u16 CSONProperty::GetU16() const { return static_cast<u16>(std::get<i64>(value)); }
    u32 CSONProperty::GetU32() const { return static_cast<u32>(std::get<i64>(value)); }
    u64 CSONProperty::GetU64() const { return static_cast<u64>(std::get<i64>(value)); }

    i16 CSONProperty::GetI16() const { return static_cast<i16>(std::get<i64>(value)); }
    i32 CSONProperty::GetI32() const { return static_cast<i32>(std::get<i64>(value)); }
    i64 CSONProperty::GetI64() const { return std::get<i64>(value); }

    bool CSONProperty::HoldsInteger() const { return std::holds_alternative<i64>(value); }

    f32 CSONProperty::GetF32() const { return static_cast<f32>(std::get<f64>(value)); }

    f64 CSONProperty::GetF64() const { return std::get<f64>(value); }
    bool CSONProperty::HoldsFloat() const { return std::holds_alternative<f64>(value); }

    bool CSONProperty::HoldsNumber() const { return HoldsFloat() || HoldsInteger(); }

    const String& CSONProperty::GetString() const { return std::get<String>(value); }
    bool CSONProperty::HoldsString() const { return std::holds_alternative<String>(value); };

    const CSONObject& CSONProperty::GetObject() const { return std::get<CSONObject>(value); }
    bool CSONProperty::HoldsObject() const { return std::holds_alternative<CSONObject>(value); }

    const CSONArray& CSONProperty::GetArray() const { return std::get<CSONArray>(value); }
    bool CSONProperty::HoldsArray() const { return std::holds_alternative<CSONArray>(value); }

    vec4 CSONProperty::GetVec4() const
    {
        if (std::holds_alternative<CSONArray>(value))
        {
            const auto& array = std::get<CSONArray>(value);

            if (array.properties.Size() != 4)
            {
                ERROR_LOG("Property: '{}' does not hold a 4 element array.", name);
                return emptyVec;
            }

            const auto x = array.properties[0].GetF64();
            const auto y = array.properties[1].GetF64();
            const auto z = array.properties[2].GetF64();
            const auto w = array.properties[3].GetF64();
            return vec4(x, y, z, w);
        }

        ERROR_LOG("Property: '{}' does not hold an array.", name);
        return emptyVec;
    }

}  // namespace C3D
