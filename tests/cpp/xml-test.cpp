#include "varn/xml/XmlModule.h"
#include "varn/xml/XmlSerializer.h"

#include <gtest/gtest.h>
#include <lua.hpp>

#include <memory>
#include <string>

namespace
{
class XmlTestHelpers
{
public:
    static std::unique_ptr<lua_State, decltype(&lua_close)> open()
    {
        std::unique_ptr<lua_State, decltype(&lua_close)> state(luaL_newstate(), &lua_close);
        luaL_openlibs(state.get());
        varn::xml::XmlModule::install(state.get());
        lua_pushcfunction(state.get(), &XmlTestHelpers::serializeArgument);
        lua_setglobal(state.get(), "serialize");
        return state;
    }

    // Runs a chunk and answers its error message, or an empty string when it succeeds.
    static std::string run(lua_State* L, const char* chunk)
    {
        if (luaL_dostring(L, chunk) == LUA_OK)
        {
            return {};
        }

        std::string message = lua_tostring(L, -1);
        lua_pop(L, 1);
        return message;
    }

    // Serializes its first argument the way an HTTP response sends a table as XML.
    static int serializeArgument(lua_State* L)
    {
        const std::string text = varn::xml::XmlSerializer::serialize(L, 1);
        lua_pushlstring(L, text.data(), text.size());
        return 1;
    }
};
} // namespace

namespace varn::xml
{

TEST(XmlSerializer, SerializeWritesEachFieldUnderTheRoot)
{
    auto state = XmlTestHelpers::open();
    lua_State* L = state.get();

    EXPECT_EQ(XmlTestHelpers::run(L, "local text = serialize({ name = 'Varn', ok = true, count = 3 })\n"
                                     "assert(text:find('<root>', 1, true), text)\n"
                                     "assert(text:find('<name>Varn</name>', 1, true), text)\n"
                                     "assert(text:find('<ok>true</ok>', 1, true), text)\n"
                                     "assert(text:find('<count>3</count>', 1, true), text)"),
              "");
}

TEST(XmlSerializer, SerializeRaisesALuaErrorThatNamesTheField)
{
    auto state = XmlTestHelpers::open();
    lua_State* L = state.get();

    const std::string nested = XmlTestHelpers::run(L, "serialize({ items = { 1, 2 } })");
    EXPECT_NE(nested.find("The field \"items\" is a table, which XML cannot carry."), std::string::npos) << nested;

    const std::string handler = XmlTestHelpers::run(L, "serialize({ on = print })");
    EXPECT_NE(handler.find("The field \"on\" is a function, which XML cannot carry."), std::string::npos) << handler;

    const std::string key = XmlTestHelpers::run(L, "serialize({ [{}] = 'x' })");
    EXPECT_NE(key.find("A field has a key of type table, which XML cannot name."), std::string::npos) << key;
}

TEST(XmlSerializer, EncodeRefusesNamesAndTextThatXmlCannotCarry)
{
    auto state = XmlTestHelpers::open();
    lua_State* L = state.get();

    const std::string markup = XmlTestHelpers::run(L, "require('xml').encode({ name = '<script>' })");
    EXPECT_NE(markup.find("The element name \"<script>\" is not a valid XML name."), std::string::npos) << markup;

    const std::string digit = XmlTestHelpers::run(L, "require('xml').encode({ name = 'a', attributes = { ['1abc'] = 'x' } })");
    EXPECT_NE(digit.find("The attribute name \"1abc\" of the element \"a\" is not a valid XML name."), std::string::npos) << digit;

    const std::string control = XmlTestHelpers::run(L, "require('xml').encode({ name = 'a', text = 'x\\1y' })");
    EXPECT_NE(control.find("The text of the element \"a\" holds the character U+0001, which XML cannot carry."), std::string::npos) << control;

    const std::string field = XmlTestHelpers::run(L, "serialize({ ['two words'] = 1 })");
    EXPECT_NE(field.find("The field name \"two words\" is not a valid XML name."), std::string::npos) << field;

    EXPECT_EQ(XmlTestHelpers::run(L, "local xml = require('xml')\n"
                                     "assert(xml.encode({ name = 'caf\\xc3\\xa9', text = 'x\\ty' }):find('<caf\\xc3\\xa9>x\\ty</caf\\xc3\\xa9>', 1, true))\n"
                                     "assert(xml.encode({ name = 'soap:Envelope', attributes = { ['xmlns:soap'] = 'urn:x' } }):find('<soap:Envelope', 1, true))"),
              "");
}

} // namespace varn::xml
