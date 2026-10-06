/*
 * This file is part of the AzerothCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "Config.h"
#include "RealmCard.h"
#include "gtest/gtest.h"
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>

namespace
{
class RealmCardConfigTest : public testing::Test
{
protected:
    void SetUp() override
    {
        configPath = (std::filesystem::temp_directory_path() / "RealmCardConfigTest.conf").string();
        std::ofstream(configPath) << "[authserver]\n"
                                     "RealmCards.Expansion = 2\n"
                                     "RealmCards.GameMode = 11\n"
                                     "RealmCards.Image = \"Default\"\n"
                                     "RealmCards.2.Expansion = 0\n"
                                     "RealmCards.2.GameMode = 12\n";
        sConfigMgr->Configure(configPath, {});
        sConfigMgr->LoadAppConfigs();
    }

    void TearDown() override
    {
        std::remove(configPath.c_str());
    }

    std::string configPath;
    RealmCardStyle const globalStyle{ 2, 11, "Default" };
};
}

TEST(RealmCardTest, FillsTheFirstPageThenTheNextOnes)
{
    EXPECT_EQ(GetRealmCardSlot(0).Page, 1);
    EXPECT_EQ(GetRealmCardSlot(0).Index, 1u);
    EXPECT_EQ(GetRealmCardSlot(5).Page, 1);
    EXPECT_EQ(GetRealmCardSlot(5).Index, 6u);
    EXPECT_EQ(GetRealmCardSlot(6).Page, 2);
    EXPECT_EQ(GetRealmCardSlot(6).Index, 1u);
    EXPECT_EQ(GetRealmCardSlot(12).Page, 3);
    EXPECT_EQ(GetRealmCardSlot(23).Index, 12u);
    EXPECT_EQ(GetRealmCardSlot(24).Page, 4);
    EXPECT_EQ(GetRealmCardSlot(24).Index, 1u);
}

TEST(RealmCardTest, NamesTheRealmThenItsCardFields)
{
    RealmCardStyle const style{ 2, 11, "Default" };
    EXPECT_EQ(BuildRealmCardName("AzerothCore", style, GetRealmCardSlot(0)), "AzerothCore!2!11!Default!1!1!1!0");
    EXPECT_EQ(BuildRealmCardName("Second", style, GetRealmCardSlot(7)), "Second!2!11!Default!1!2!2!0");
}

TEST_F(RealmCardConfigTest, GivesARealmWithoutItsOwnKeysTheGlobalCard)
{
    RealmCardStyle const style = GetRealmCardStyle(1, globalStyle);
    EXPECT_EQ(style.Expansion, 2u);
    EXPECT_EQ(style.GameMode, 11u);
    EXPECT_EQ(style.Image, "Default");
}

TEST_F(RealmCardConfigTest, TakesEachKeyARealmSetsAndTheGlobalForTheRest)
{
    RealmCardStyle const style = GetRealmCardStyle(2, globalStyle);
    EXPECT_EQ(style.Expansion, 0u);
    EXPECT_EQ(style.GameMode, 12u);
    EXPECT_EQ(style.Image, "Default");
}

TEST_F(RealmCardConfigTest, ReadsARealmKeyFromItsDoubleUnderscoreEnvironmentVariable)
{
    setenv("AC_REALM_CARDS_9__IMAGE", "Vanilla", 1);
    RealmCardStyle const style = GetRealmCardStyle(9, globalStyle);
    EXPECT_EQ(style.Image, "Vanilla");
    EXPECT_EQ(style.Expansion, 2u);
}
