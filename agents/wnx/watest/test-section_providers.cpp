// Copyright (C) 2019 tribe29 GmbH - License: GNU General Public License v2
// This file is part of Checkmk (https://checkmk.com). It is subject to the
// terms and conditions defined in the file COPYING, which is part of this
// source code package.

#include "pch.h"

#include "cfg.h"
#include "common/version.h"
#include "common/wtools.h"
#include "providers/check_mk.h"
#include "providers/df.h"
#include "providers/internal.h"
#include "providers/logwatch_event.h"
#include "providers/mem.h"
#include "providers/mrpe.h"
#include "providers/ohm.h"
#include "providers/p_perf_counters.h"
#include "providers/plugins.h"
#include "providers/services.h"
#include "providers/skype.h"
#include "providers/wmi.h"
#include "service_processor.h"
#include "test_tools.h"
#include "tools/_misc.h"
#include "tools/_process.h"

namespace cma::provider {

static const std::string section_name{cma::section::kUseEmbeddedName};

class Empty : public Synchronous {
public:
    Empty() : Synchronous("empty") {}
    std::string makeBody() override { return "****"; }
};

TEST(SectionProviders, Basic) {
    Empty e;

    EXPECT_TRUE(e.getHostSp() == nullptr);
    cma::srv::ServiceProcessor sp;
    e.host_sp_ = &sp;
    EXPECT_EQ(e.getHostSp(), &sp);
}

TEST(SectionProviders, Construction) {
    PluginsProvider plugins;
    EXPECT_EQ(plugins.getUniqName(), cma::section::kPlugins);

    LocalProvider local;
    EXPECT_EQ(local.getUniqName(), cma::section::kLocal);
}

TEST(SectionProviders, BasicUptime) {
    using namespace cma::section;
    using namespace cma::provider;

    cma::srv::SectionProvider<UptimeSync> uptime_provider;
    EXPECT_EQ(uptime_provider.getEngine().getUniqName(), kUptimeName);

    auto& e3 = uptime_provider.getEngine();
    auto uptime = e3.generateContent(section_name);
    ASSERT_TRUE(!uptime.empty());
    auto result = cma::tools::SplitString(uptime, "\n");
    EXPECT_EQ(result.size(), 2);
    EXPECT_EQ(result[0], "<<<uptime>>>");
    auto value = result[1].find_first_not_of("0123456789");
    EXPECT_EQ(value, std::string::npos);
}

TEST(SectionProviders, BasicDf) {
    using namespace cma::section;
    using namespace cma::provider;

    cma::srv::SectionProvider<Df> df_provider;
    EXPECT_EQ(df_provider.getEngine().getUniqName(), kDfName);

    auto& e2 = df_provider.getEngine();
    auto df = e2.generateContent(section_name);
    ASSERT_TRUE(!df.empty());
    auto result = cma::tools::SplitString(df, "\n");
    ASSERT_TRUE(result.size() > 1);
    EXPECT_EQ(result[0], "<<<df:sep(9)>>>");
    auto count = result.size();
    for (size_t i = 1; i < count; ++i) {
        auto values = cma::tools::SplitString(result[i], "\t");
        ASSERT_EQ(values.size(), 7);

        auto ret = values[2].find_first_not_of("0123456789");
        EXPECT_EQ(ret, std::string::npos);

        ret = values[3].find_first_not_of("0123456789");
        EXPECT_EQ(ret, std::string::npos);

        ret = values[4].find_first_not_of("0123456789");
        EXPECT_EQ(ret, std::string::npos);

        EXPECT_EQ(values[5].back(), '%');
    }
}

TEST(SectionProviders, SystemTime) {
    auto seconds_since_epoch = tools::SecondsSinceEpoch();
    srv::SectionProvider<SystemTime> system_time_provider;
    auto& engine = system_time_provider.getEngine();

    EXPECT_EQ(engine.getUniqName(), section::kSystemTime);

    auto system_time = engine.generateContent(section_name);
    EXPECT_EQ(system_time.back(), '\n');

    auto result = tools::SplitString(system_time, "\n");
    ASSERT_EQ(result.size(), 2);
    EXPECT_EQ(result[0], "<<<systemtime>>>");
    auto value = std::stoll(result[1]);
    EXPECT_GE(value, seconds_since_epoch);
}

class SectionProviderCheckMkFixture : public ::testing::Test {
public:
    static constexpr std::string_view names_[] = {
        "Version",          "BuildDate",       "AgentOS",
        "Hostname",         "Architecture",    "WorkingDirectory",
        "ConfigFile",       "LocalConfigFile", "AgentDirectory",
        "PluginsDirectory", "StateDirectory",  "ConfigDirectory",
        "TempDirectory",    "LogDirectory",    "SpoolDirectory",
        "LocalDirectory",   "OnlyFrom"};

    static constexpr std::pair<std::string_view, std::string_view>
        only_from_cases_[] = {
            //
            {"~", ""},
            {"127.0.0.1", "127.0.0.1"},
            {"127.0.0.1 192.168.0.1", "127.0.0.1 192.168.0.1"},
            {"[127.0.0.1, 192.168.0.1]", "127.0.0.1 192.168.0.1"},
            {"[127.0.0.1, ::1]", "127.0.0.1 ::1"},
            {"[127.0.0.1/16, ::1/64]", "127.0.0.1/16 ::1/64"}
            //
        };

    std::string getContent() { return getEngine().generateContent(); }
    std::vector<std::string> getResultAsTable() {
        return tools::SplitString(getContent(), "\n");
    }
    CheckMk& getEngine() { return check_mk_provider_.getEngine(); }

    YAML::Node getWorkingCfg() {
        if (!temp_fs_) {
            temp_fs_ = std::move(tst::TempCfgFs::CreateNoIo());
        }
        return cfg::GetLoadedConfig();
    }

private:
    srv::SectionProvider<CheckMk> check_mk_provider_;
    tst::TempCfgFs::ptr temp_fs_;
};

TEST_F(SectionProviderCheckMkFixture, Name) {
    EXPECT_EQ(getEngine().getUniqName(), section::kCheckMk);
}

TEST_F(SectionProviderCheckMkFixture, ConstFields) {
    auto cfg = getWorkingCfg();
    cfg[cfg::groups::kGlobal][cfg::vars::kOnlyFrom] = YAML::Load("127.0.0.1");

    auto result = getResultAsTable();
    EXPECT_EQ(result.size(), 18);
    EXPECT_EQ(result[0] + "\n", section::MakeHeader(section::kCheckMk));
    result.erase(result.begin());  // kill header

    const auto* expected_name = names_;
    for (const auto& r : result) {
        auto values = tools::SplitString(r, ": ");
        EXPECT_EQ(values.size(), 2);
        EXPECT_EQ(values[0], std::string{*expected_name++});
        EXPECT_FALSE(values[1].empty());
    }
}

TEST_F(SectionProviderCheckMkFixture, AdvancedFields) {
    auto result = getResultAsTable();
    auto get_val = [](const std::string& raw) -> std::string {
        auto tbl = tools::SplitString(raw, ": ");
        return tbl[1];
    };

    EXPECT_EQ(get_val(result[1]), CHECK_MK_VERSION);
    EXPECT_EQ(get_val(result[3]), "windows");
    EXPECT_EQ(get_val(result[4]), cfg::GetHostName());
    if constexpr (tgt::Is64bit())
        EXPECT_EQ(get_val(result[5]), "64bit");
    else
        EXPECT_EQ(get_val(result[5]), "32bit");
}

TEST_F(SectionProviderCheckMkFixture, OnlyFromField) {
    auto cfg = getWorkingCfg();

    const auto offset_to_only_from = getContent().size() - 1;  // remove \n
    for (auto p : only_from_cases_) {
        cfg[cfg::groups::kGlobal][cfg::vars::kOnlyFrom] =
            YAML::Load(std::string{p.first});
        auto result = getContent().substr(offset_to_only_from);
        EXPECT_EQ(result, std::string{p.second} + "\n");
    }
}

class SectionProvidersFixture : public ::testing::Test {
public:
    Services& getEngine() { return services_provider.getEngine(); }

private:
    srv::SectionProvider<Services> services_provider;
};

TEST_F(SectionProvidersFixture, ServicesCtor) {
    EXPECT_EQ(getEngine().getUniqName(), section::kServices);
}
TEST_F(SectionProvidersFixture, ServicesIntegration) {
    auto content = getEngine().generateContent(section_name);

    // Validate content is presented and correct
    ASSERT_TRUE(!content.empty());
    auto result = tools::SplitString(content, "\n");
    EXPECT_TRUE(result.size() > 20);

    // Validate Header
    EXPECT_EQ(result[0], "<<<services>>>");

    // Validate Body
    auto count = result.size();
    for (size_t i = 1; i < count; ++i) {
        auto values = tools::SplitString(result[i], " ", 2);
        EXPECT_FALSE(values[0].empty());
        EXPECT_FALSE(values[1].empty());
        EXPECT_FALSE(values[2].empty());
        EXPECT_TRUE(values[1].find("/") != std::string::npos);
    }
}

TEST(SectionHeaders, All) {
    auto ret = cma::section::MakeHeader("x");
    EXPECT_EQ(ret, "<<<x>>>\n");

    ret = cma::section::MakeHeader("x", ',');
    EXPECT_EQ(ret, "<<<x:sep(44)>>>\n");

    ret = cma::section::MakeHeader("x", '\t');
    EXPECT_EQ(ret, "<<<x:sep(9)>>>\n");

    ret = cma::section::MakeHeader("x", '\0');
    EXPECT_EQ(ret, "<<<x>>>\n");

    ret = cma::section::MakeHeader("", '\0');
    EXPECT_EQ(ret, "<<<nothing>>>\n");

    ret = cma::section::MakeSubSectionHeader("x");
    EXPECT_EQ(ret, "[x]\n");

    ret = cma::section::MakeSubSectionHeader("");
    EXPECT_EQ(ret, "[nothing]\n");

    ret = cma::section::MakeEmptyHeader();
    EXPECT_EQ(ret, "<<<>>>\n");

    ret = cma::section::MakeLocalHeader();
    EXPECT_EQ(ret, "<<<local:sep(0)>>>\n");
}

}  // namespace cma::provider
