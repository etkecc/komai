// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include <iostream>

#include "settings/StagedLoadPlan.h"
#include "TestEnvironment.h"

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
testStagedLoadPlan()
{
    bool ok = true;

    const auto fileProvider =
      staged_load_plan::providerFromConfigValue(QStringLiteral("file"));
    ok &= expect(fileProvider == staged_load_plan::SecretsProvider::File,
                 "providerFromConfigValue resolves file provider");

    const auto fileStages = staged_load_plan::stagesForProvider(fileProvider);
    ok &= expect(fileStages.size() == 4, "file provider stages count");
    ok &= expect(fileStages.at(0) == staged_load_plan::Stage::Config, "file stages[0]=config");
    ok &= expect(fileStages.at(1) == staged_load_plan::Stage::Session, "file stages[1]=session");
    ok &= expect(fileStages.at(2) == staged_load_plan::Stage::SecretsFile,
                 "file stages[2]=secrets_file");
    ok &= expect(fileStages.at(3) == staged_load_plan::Stage::State, "file stages[3]=state");

    const auto defaultProvider = staged_load_plan::providerFromConfigValue(QStringLiteral(""));
    ok &= expect(defaultProvider == staged_load_plan::SecretsProvider::SecretService,
                 "providerFromConfigValue defaults to secret_service");

    const auto secureStages = staged_load_plan::stagesForProvider(defaultProvider);
    ok &= expect(secureStages.size() == 4, "secure provider stages count");
    ok &= expect(secureStages.at(0) == staged_load_plan::Stage::Config, "secure stages[0]=config");
    ok &= expect(secureStages.at(1) == staged_load_plan::Stage::Session,
                 "secure stages[1]=session");
    ok &= expect(secureStages.at(2) == staged_load_plan::Stage::SecretsSecureBackend,
                 "secure stages[2]=secrets_secure_backend");
    ok &= expect(secureStages.at(3) == staged_load_plan::Stage::State, "secure stages[3]=state");

    return ok;
}

} // namespace

int
main()
{
    test_env::ScopedTestHome testHome{QStringLiteral("komai-staged-load-plan-test")};
    if (!testHome.isValid()) {
        std::cerr << "FAILED: test home environment can be created\n";
        return 1;
    }
    if (!testHome.isIsolated()) {
        std::cerr << "FAILED: test home environment is isolated\n";
        return 1;
    }

    bool ok = true;
    ok &= testStagedLoadPlan();
    return ok ? 0 : 1;
}
