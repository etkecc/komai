// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include <iostream>

#include "settings/core/SettingsSerializer.h"

namespace {

using settings::core::SettingId;
using settings::core::SettingsStore;
namespace serializer = settings::core::serializer;

bool
expect(bool condition, const char *message)
{
    if (condition)
        return true;

    std::cerr << "FAILED: " << message << '\n';
    return false;
}

bool
testKindDetection()
{
    bool ok = true;
    ok &= expect(serializer::kindOf(SettingsStore::Value{true}) == serializer::ValueKind::Bool,
                 "kindOf(bool)");
    ok &= expect(serializer::kindOf(SettingsStore::Value{42}) == serializer::ValueKind::Int,
                 "kindOf(int)");
    ok &= expect(serializer::kindOf(SettingsStore::Value{1.25}) == serializer::ValueKind::Double,
                 "kindOf(double)");
    ok &= expect(serializer::kindOf(SettingsStore::Value{std::string("komai")}) ==
                   serializer::ValueKind::String,
                 "kindOf(string)");
    ok &= expect(
      serializer::kindOf(SettingsStore::Value{SettingsStore::StringList{"a", "b"}}) ==
        serializer::ValueKind::StringList,
      "kindOf(string-list)");
    return ok;
}

bool
testYamlRoundtrip()
{
    bool ok = true;

    const auto boolNode = serializer::toYamlNode(SettingsStore::Value{true});
    const auto boolValue = serializer::fromYamlNode(boolNode, serializer::ValueKind::Bool);
    ok &= expect(boolValue.has_value() && std::get<bool>(*boolValue), "bool roundtrip");

    const auto intNode = serializer::toYamlNode(SettingsStore::Value{900});
    const auto intValue = serializer::fromYamlNode(intNode, serializer::ValueKind::Int);
    ok &= expect(intValue.has_value() && std::get<int>(*intValue) == 900, "int roundtrip");

    const auto doubleNode = serializer::toYamlNode(SettingsStore::Value{1.5});
    const auto doubleValue = serializer::fromYamlNode(doubleNode, serializer::ValueKind::Double);
    ok &= expect(doubleValue.has_value() && std::get<double>(*doubleValue) == 1.5,
                 "double roundtrip");

    const auto stringNode = serializer::toYamlNode(SettingsStore::Value{std::string("value")});
    const auto stringValue = serializer::fromYamlNode(stringNode, serializer::ValueKind::String);
    ok &= expect(stringValue.has_value() && std::get<std::string>(*stringValue) == "value",
                 "string roundtrip");

    const auto listNode =
      serializer::toYamlNode(SettingsStore::Value{SettingsStore::StringList{"x", "y", "z"}});
    const auto listValue = serializer::fromYamlNode(listNode, serializer::ValueKind::StringList);
    ok &= expect(listValue.has_value(), "string-list roundtrip parses");
    if (listValue.has_value()) {
        const auto &list = std::get<SettingsStore::StringList>(*listValue);
        ok &= expect(list.size() == 3 && list.at(2) == "z", "string-list roundtrip values");
    }

    return ok;
}

bool
testFallbackBehavior()
{
    const YAML::Node root = YAML::Load("{width: abc, theme: komai-light}");
    const auto defaultWidth = SettingsStore::Value{1024};
    const auto widthValue = serializer::fromYamlNodeOrDefault(root["width"], defaultWidth);
    const auto missingValue = serializer::fromYamlNodeOrDefault(root["missing"], defaultWidth);
    const auto themeDefault = SettingsStore::Value{std::string("komai")};
    const auto themeValue = serializer::fromYamlNodeOrDefault(root["theme"], themeDefault);

    bool ok = true;
    ok &= expect(std::holds_alternative<int>(widthValue) && std::get<int>(widthValue) == 1024,
                 "invalid type falls back to typed default");
    ok &= expect(std::holds_alternative<int>(missingValue) && std::get<int>(missingValue) == 1024,
                 "missing value falls back to typed default");
    ok &= expect(
      std::holds_alternative<std::string>(themeValue) && std::get<std::string>(themeValue) == "komai-light",
      "valid value overrides default");
    return ok;
}

bool
testSetFromYamlNodeOrDefault()
{
    const YAML::Node root = YAML::Load("{scale: 2.0, badScale: no}");

    SettingsStore store;
    const auto first = serializer::setFromYamlNodeOrDefault(
      store, SettingId::UiFontSizePt, root["scale"], SettingsStore::Value{1.0});

    bool ok = true;
    ok &= expect(first.success && first.changed, "set from valid YAML succeeds");
    const auto asDouble = store.valueAs<double>(SettingId::UiFontSizePt);
    ok &= expect(asDouble.has_value() && *asDouble == 2.0, "store keeps parsed YAML value");

    const auto second = serializer::setFromYamlNodeOrDefault(
      store, SettingId::UiFontSizePt, root["badScale"], SettingsStore::Value{1.0});
    ok &= expect(second.success && second.changed, "set from invalid YAML applies fallback");
    const auto asDoubleFallback = store.valueAs<double>(SettingId::UiFontSizePt);
    ok &= expect(asDoubleFallback.has_value() && *asDoubleFallback == 1.0,
                 "store keeps typed default on parse mismatch");

    return ok;
}

} // namespace

int
main()
{
    bool ok = true;
    ok &= testKindDetection();
    ok &= testYamlRoundtrip();
    ok &= testFallbackBehavior();
    ok &= testSetFromYamlNodeOrDefault();
    return ok ? 0 : 1;
}
