/* Copyright (C) 2016+ AzerothCore, GNU AGPL v3. */

#include "AscensionFreepickRules.h"
#include "RealmsAdvancement.h"
#include "gtest/gtest.h"

using namespace AscensionFreepick;

namespace
{
constexpr std::uint32_t WARRIOR = 1;
constexpr std::uint32_t MAGE = 8;
constexpr std::uint32_t HERO_TREE = 3;
constexpr std::uint32_t COA_TREE = 14;
constexpr std::uint32_t REBORN_WARRIOR = 37;
constexpr std::uint32_t FURY = 5;
constexpr std::uint32_t HERO_ROW = 1000;
constexpr std::uint32_t COA_ROW = 2000;
constexpr std::uint32_t CRUELTY = 12380;
constexpr std::uint32_t BOOMING_VOICE = 12378;
constexpr std::uint32_t RETAINED_ROW = 3000;
constexpr std::uint32_t LEVEL_20_UNLEARN = 71 * 20;

Row TalentRow(std::uint32_t entryId, std::uint32_t classType, std::uint32_t tab, std::uint32_t ranks,
    std::uint32_t level)
{
    Row row;
    row.EntryId = entryId;
    row.Type = ENTRY_TALENT;
    row.ClassType = classType;
    row.Tab = tab;
    row.TECost = 1;
    row.RequiredLevel = level;
    row.Realms[0] = true;
    for (std::uint32_t rank = 0; rank < ranks; ++rank)
        row.Spells[rank] = entryId * 10 + rank + 1;
    return row;
}

class RealmsAdvancementTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        catalog.ClassTypes[HERO_TREE] = { HERO_CLASS, true, false, false };
        catalog.ClassTypes[COA_TREE] = { 12, false, true, false };
        catalog.ClassTypes[REBORN_WARRIOR] = { WARRIOR, false, false, true };
        Add(TalentRow(HERO_ROW, HERO_TREE, 1, 1, 10));
        Add(TalentRow(COA_ROW, COA_TREE, 1, 1, 10));
        Add(TalentRow(CRUELTY, REBORN_WARRIOR, FURY, 5, 10));
        Add(TalentRow(BOOMING_VOICE, REBORN_WARRIOR, FURY, 2, 10));
        Row retained = TalentRow(RETAINED_ROW, REBORN_WARRIOR, FURY, 1, 10);
        retained.Flags = ROW_RETAINED;
        Add(retained);
        for (std::uint32_t level = 1; level <= 80; ++level)
        {
            catalog.Budget[HERO_CLASS].push_back({ level, 20 + level, level > 9 ? level - 9 : 0 });
            catalog.Budget[WARRIOR].push_back({ level, level, level > 9 ? level - 9 : 0 });
        }
        hero.Live = true;
        reborn.Live = true;
        reborn.WarcraftReborn = true;
    }

    void Add(Row row)
    {
        catalog.RowOrder.push_back(row.EntryId);
        catalog.Rows.emplace(row.EntryId, std::move(row));
    }

    Catalog catalog;
    Realm hero;
    Realm reborn;
};

TEST_F(RealmsAdvancementTest, BudgetIsLookedUpForTheBuildsClass)
{
    EXPECT_EQ(Build(catalog, reborn, 20, {}, WARRIOR).TEBudget(), 11u);
    EXPECT_EQ(Build(catalog, reborn, 20, {}, WARRIOR).AEBudget(), 20u);
    EXPECT_EQ(Build(catalog, hero, 20, {}, HERO_CLASS).AEBudget(), 40u);
    EXPECT_EQ(Build(catalog, reborn, 20, {}, MAGE).TEBudget(), 0u);
}

TEST_F(RealmsAdvancementTest, WrongClassRuleAdmitsTheRowsOwnClassOnly)
{
    EXPECT_EQ(Build(catalog, reborn, 20, {}, WARRIOR).ValidateLearn(CRUELTY, nullptr), std::uint32_t(LEARN_OK));
    EXPECT_EQ(Build(catalog, reborn, 20, {}, MAGE).ValidateLearn(CRUELTY, nullptr), std::uint32_t(LEARN_WRONG_CLASS));
}

TEST_F(RealmsAdvancementTest, WarcraftRebornHidesHeroAndCoARows)
{
    EXPECT_TRUE(Visible(catalog, reborn, catalog.Rows.at(CRUELTY)));
    EXPECT_FALSE(Visible(catalog, reborn, catalog.Rows.at(HERO_ROW)));
    EXPECT_FALSE(Visible(catalog, reborn, catalog.Rows.at(COA_ROW)));
    EXPECT_TRUE(Visible(catalog, hero, catalog.Rows.at(HERO_ROW)));
}

TEST_F(RealmsAdvancementTest, PurgePricesEachRemovedEntryAtTheUnlearnCost)
{
    Build const build(catalog, reborn, 20, { { CRUELTY, 3 }, { BOOMING_VOICE, 2 }, { RETAINED_ROW, 1 } }, WARRIOR);
    ApplyCheck const paid = CheckPurge(build, nullptr, { 2 * LEVEL_20_UNLEARN, 0 });
    EXPECT_EQ(paid.Result, UPDATE_OK);
    EXPECT_EQ(paid.Money, 2 * LEVEL_20_UNLEARN);
    ASSERT_EQ(paid.Entries.size(), 1u);
    EXPECT_EQ(paid.Entries[0].EntryId, RETAINED_ROW);
}

TEST_F(RealmsAdvancementTest, PurgeSpendsMarksBeforeMoney)
{
    Build const build(catalog, reborn, 20, { { CRUELTY, 3 }, { BOOMING_VOICE, 2 } }, WARRIOR);
    ApplyCheck const paid = CheckPurge(build, nullptr, { 0, 500 });
    EXPECT_EQ(paid.Result, UPDATE_OK);
    EXPECT_EQ(paid.Marks, 500u);
    EXPECT_EQ(paid.Money, 0u);
}

TEST_F(RealmsAdvancementTest, PurgeRefusesAPurseThatCannotPay)
{
    Build const build(catalog, reborn, 20, { { CRUELTY, 3 }, { BOOMING_VOICE, 2 } }, WARRIOR);
    EXPECT_EQ(CheckPurge(build, nullptr, { 2 * LEVEL_20_UNLEARN - 1, 0 }).Result, UPDATE_BAD_UPDATE_COSTS);
}

TEST_F(RealmsAdvancementTest, PurgeIsFreeUpToLevelTenAndEmptyWithNothingToRemove)
{
    ApplyCheck const free = CheckPurge(Build(catalog, reborn, 10, { { CRUELTY, 1 } }, WARRIOR), nullptr, {});
    EXPECT_EQ(free.Result, UPDATE_OK);
    EXPECT_EQ(free.Money + free.Marks, 0u);
    EXPECT_EQ(CheckPurge(Build(catalog, reborn, 20, { { RETAINED_ROW, 1 } }, WARRIOR), nullptr, {}).Result,
        UPDATE_NO_DIFF);
}

TEST_F(RealmsAdvancementTest, SelfReportCountsStockRowsAndBudgetedStockClasses)
{
    EXPECT_EQ(Realms::StockCatalogRows(catalog), 3u);
    EXPECT_EQ(Realms::BudgetedStockClasses(catalog), 1u);
}
}
