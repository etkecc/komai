// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include <iostream>

#include "settings/YamlSettings.h"

namespace {

bool
expect(bool condition, const char *message)
{
    if (condition)
        return true;

    std::cerr << "FAILED: " << message << '\n';
    return false;
}

bool
testYamlHelpers()
{
    bool ok = true;

    YAML::Node root = YAML::Load(R"(
session:
  account:
    user_id: "@alice:example.com"
    homeserver: https://matrix.example.com:443
  device:
    id: EXAMPLEDEVICE1
  presence:
    default: automatic_presence
)");

    const auto userId1 = yaml_settings::readString(root, "session.account.user_id", QString{});
    const auto device1 = yaml_settings::readString(root, "session.device.id", QString{});
    const auto server1 = yaml_settings::readString(root, "session.account.homeserver", QString{});
    const auto userId2 = yaml_settings::readString(root, "session.account.user_id", QString{});

    ok &= expect(userId1 == QLatin1String("@alice:example.com"), "first user_id read");
    ok &= expect(device1 == QLatin1String("EXAMPLEDEVICE1"), "first device.id read");
    ok &= expect(server1 == QLatin1String("https://matrix.example.com:443"), "first homeserver read");
    ok &= expect(userId2 == QLatin1String("@alice:example.com"), "repeated user_id read is stable");

    YAML::Node writeRoot(YAML::NodeType::Map);
    yaml_settings::setNode(writeRoot, "a.b.c", 42);
    yaml_settings::setNode(writeRoot, "a.b.d", std::string("hello"));
    ok &= expect(yaml_settings::readScalar<int>(writeRoot, "a.b.c", -1) == 42,
                 "setNode/readScalar integer roundtrip");
    ok &= expect(yaml_settings::readString(writeRoot, "a.b.d", QString{}) == QLatin1String("hello"),
                 "setNode/readString text roundtrip");

    return ok;
}

} // namespace

int
main()
{
    bool ok = true;
    ok &= testYamlHelpers();
    return ok ? 0 : 1;
}
