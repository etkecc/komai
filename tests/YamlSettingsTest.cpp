// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include <iostream>
#include <string>

#include "settings/StagedLoadPlan.h"
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

} // namespace

int
main()
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

    YAML::Node configFileProvider = YAML::Load(R"(
secrets:
  provider: file
)");

    const auto fileProvider = staged_load_plan::providerFromConfig(configFileProvider);
    ok &= expect(fileProvider == staged_load_plan::SecretsProvider::File,
                 "providerFromConfig resolves file provider");

    const auto fileStages = staged_load_plan::stagesForProvider(fileProvider);
    ok &= expect(fileStages.size() == 4, "file provider stages count");
    ok &= expect(fileStages.at(0) == staged_load_plan::Stage::Config, "file stages[0]=config");
    ok &= expect(fileStages.at(1) == staged_load_plan::Stage::Session, "file stages[1]=session");
    ok &= expect(fileStages.at(2) == staged_load_plan::Stage::SecretsFile,
                 "file stages[2]=secrets_file");
    ok &= expect(fileStages.at(3) == staged_load_plan::Stage::State, "file stages[3]=state");

    YAML::Node configDefault = YAML::Load(R"(
ui:
  theme:
    slug: komai
)");

    const auto defaultProvider = staged_load_plan::providerFromConfig(configDefault);
    ok &= expect(defaultProvider == staged_load_plan::SecretsProvider::SecretService,
                 "providerFromConfig defaults to secret_service");

    const auto secureStages = staged_load_plan::stagesForProvider(defaultProvider);
    ok &= expect(secureStages.size() == 4, "secure provider stages count");
    ok &= expect(secureStages.at(0) == staged_load_plan::Stage::Config, "secure stages[0]=config");
    ok &= expect(secureStages.at(1) == staged_load_plan::Stage::Session,
                 "secure stages[1]=session");
    ok &= expect(secureStages.at(2) == staged_load_plan::Stage::SecretsSecureBackend,
                 "secure stages[2]=secrets_secure_backend");
    ok &= expect(secureStages.at(3) == staged_load_plan::Stage::State, "secure stages[3]=state");

    return ok ? 0 : 1;
}
