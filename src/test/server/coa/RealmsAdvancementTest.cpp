/* Copyright (C) 2016+ AzerothCore, GNU AGPL v3. */

#include "AscensionFreepickRules.h"
#include "RealmsAdvancement.h"
#include "gtest/gtest.h"
#include <algorithm>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>
#include <string_view>

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
constexpr std::uint32_t MASTERY_ROW = 4000;
constexpr std::uint32_t MEMBER_ROW = 4001;
constexpr std::uint32_t LATE_MEMBER_ROW = 4002;
constexpr std::uint32_t LEVEL_20_SINK_ROW = 4003;
constexpr std::uint32_t LEVEL_20_AE = 40;
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

Row AbilityRow(std::uint32_t entryId, std::uint32_t classType, std::uint32_t level, std::uint32_t flags)
{
    Row row;
    row.EntryId = entryId;
    row.Type = ENTRY_ABILITY;
    row.ClassType = classType;
    row.AECost = 1;
    row.RequiredLevel = level;
    row.Flags = flags;
    row.Realms[0] = true;
    row.Spells[0] = entryId * 10 + 1;
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
        Add(AbilityRow(MASTERY_ROW, HERO_TREE, 1, ROW_MASTERY));
        Row member = AbilityRow(MEMBER_ROW, HERO_TREE, 1, ROW_AUTOMATIC);
        member.Masteries[0] = MASTERY_ROW;
        Add(member);
        Row lateMember = AbilityRow(LATE_MEMBER_ROW, HERO_TREE, 20, ROW_AUTOMATIC);
        lateMember.Masteries[0] = MASTERY_ROW;
        Add(lateMember);
        Row sink = AbilityRow(LEVEL_20_SINK_ROW, HERO_TREE, 1, 0);
        sink.AECost = LEVEL_20_AE - 1;
        Add(sink);
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

TEST_F(RealmsAdvancementTest, TreeTabsFollowTheTalentPagesByName)
{
    Realms::TreeTabMatch const hunter = Realms::MatchTreeTabs(
        { { 2, "Survival" }, { 3, "Marksmanship" }, { 4, "BeastMastery" } },
        { "Beast Mastery", "Marksmanship", "Survival" });
    EXPECT_TRUE(hunter.ByName);
    EXPECT_EQ(hunter.Tabs, (Realms::TreeTabIds{ 4, 3, 2 }));
}

TEST_F(RealmsAdvancementTest, TreeTabsGiveAMisspelledPageTheTabNoOtherPageTook)
{
    Realms::TreeTabMatch const rogue = Realms::MatchTreeTabs(
        { { 8, "Combat" }, { 9, "Subtlety" }, { 10, "Assassination" } }, { "Assassination", "Combat", "Subtley" });
    EXPECT_FALSE(rogue.ByName);
    EXPECT_EQ(rogue.Tabs, (Realms::TreeTabIds{ 10, 8, 9 }));
}

TEST_F(RealmsAdvancementTest, AMasteryGrantsItsMembersForItsOwnCost)
{
    Build const base(catalog, hero, 10);
    ApplyCheck const check = CheckApply(base, { { MASTERY_ROW, 1 } }, nullptr, {});
    ASSERT_EQ(check.Result, UPDATE_OK);
    Build built(base);
    built.SetEntries(check.Entries);
    built.AutoLearn(nullptr);
    EXPECT_EQ(built.Entries().size(), 2u);
    EXPECT_TRUE(built.Has(MASTERY_ROW));
    EXPECT_TRUE(built.Has(MEMBER_ROW));
    EXPECT_EQ(built.GlobalAE(0), 1u);
    EXPECT_EQ(built.RemainingAE(), 29u);
}

TEST_F(RealmsAdvancementTest, MembersListedBeforeTheirMasteryAreAccepted)
{
    Build const base(catalog, hero, 10);
    ApplyCheck const check = CheckApply(base, { { MEMBER_ROW, 1 }, { MASTERY_ROW, 1 } }, nullptr, {});
    ASSERT_EQ(check.Result, UPDATE_OK);
    EXPECT_EQ(Build(catalog, hero, 10, check.Entries).GlobalAE(0), 1u);
}

TEST_F(RealmsAdvancementTest, ALateMemberWaitsForItsLevel)
{
    Build early(catalog, hero, 10, { { MASTERY_ROW, 1 } });
    EXPECT_EQ(early.AutoLearn(nullptr), 1u);
    EXPECT_TRUE(early.Has(MEMBER_ROW));
    EXPECT_FALSE(early.Has(LATE_MEMBER_ROW));
    Build later(catalog, hero, 20, early.Entries());
    EXPECT_EQ(later.AutoLearn(nullptr), 1u);
    EXPECT_TRUE(later.Has(LATE_MEMBER_ROW));
    EXPECT_EQ(later.GlobalAE(0), 1u);
}

TEST_F(RealmsAdvancementTest, AMemberBoughtBeforeItsMasteryBecomesFree)
{
    Build const base(catalog, hero, 10, { { MEMBER_ROW, 1 } });
    EXPECT_EQ(base.GlobalAE(0), 1u);
    ApplyCheck const check = CheckApply(base, { { MEMBER_ROW, 1 }, { MASTERY_ROW, 1 } }, nullptr, {});
    ASSERT_EQ(check.Result, UPDATE_OK);
    EXPECT_EQ(Build(catalog, hero, 10, check.Entries).GlobalAE(0), 1u);
}

TEST_F(RealmsAdvancementTest, RemovingAMasteryPricesTheMasteryNotItsMembers)
{
    Build const base(catalog, hero, 20, { { MASTERY_ROW, 1 }, { MEMBER_ROW, 1 } });
    ApplyCheck const check = CheckApply(base, {}, nullptr, { 2 * LEVEL_20_UNLEARN, 0 });
    ASSERT_EQ(check.Result, UPDATE_OK);
    EXPECT_EQ(check.Money, LEVEL_20_UNLEARN);
    EXPECT_TRUE(check.Entries.empty());
}

TEST_F(RealmsAdvancementTest, AMemberLeftOutWhenItsMasteryIsBoughtLeavesFree)
{
    Build const base(catalog, hero, 20, { { MEMBER_ROW, 1 } });
    ApplyCheck const check = CheckApply(base, { { MASTERY_ROW, 1 } }, nullptr, { LEVEL_20_UNLEARN, 0 });
    ASSERT_EQ(check.Result, UPDATE_OK);
    EXPECT_EQ(check.Money, 0u);
    EXPECT_EQ(check.Marks, 0u);
    Build built(base);
    built.SetEntries(check.Entries);
    built.AutoLearn(nullptr);
    EXPECT_TRUE(built.Has(MEMBER_ROW));
}

TEST_F(RealmsAdvancementTest, ALateMemberArrivesWithNoEssenceLeft)
{
    Build build(catalog, hero, 20, { { MASTERY_ROW, 1 }, { MEMBER_ROW, 1 }, { LEVEL_20_SINK_ROW, 1 } });
    ASSERT_EQ(build.RemainingAE(), 0u);
    EXPECT_EQ(build.AutoLearn(nullptr), 1u);
    EXPECT_TRUE(build.Has(LATE_MEMBER_ROW));
}

TEST_F(RealmsAdvancementTest, MembersBeforeTheirMasteryOnAFullBudgetAreReordered)
{
    ApplyCheck const check = CheckApply(Build(catalog, hero, 20),
        { { MEMBER_ROW, 1 }, { LEVEL_20_SINK_ROW, 1 }, { MASTERY_ROW, 1 } }, nullptr, {});
    ASSERT_EQ(check.Result, UPDATE_OK);
    ASSERT_EQ(check.Entries.size(), 3u);
    EXPECT_EQ(check.Entries[0].EntryId, MASTERY_ROW);
    EXPECT_EQ(Build(catalog, hero, 20, check.Entries).RemainingAE(), 0u);
}

TEST_F(RealmsAdvancementTest, ADroppedMemberReturnsWhileItsMasteryStays)
{
    Build const base(catalog, hero, 10, { { MASTERY_ROW, 1 }, { MEMBER_ROW, 1 } });
    ApplyCheck const check = CheckApply(base, { { MASTERY_ROW, 1 } }, nullptr, {});
    ASSERT_EQ(check.Result, UPDATE_OK);
    Build built(base);
    built.SetEntries(check.Entries);
    built.AutoLearn(nullptr);
    EXPECT_TRUE(built.Has(MEMBER_ROW));
}

TEST_F(RealmsAdvancementTest, PricingLiveMembersFreesThemAndRequiresTheirMastery)
{
    constexpr std::uint32_t ABSENT_MASTERY = 4900;
    constexpr std::uint32_t ORPHAN_MEMBER_ROW = 4004;
    constexpr std::uint32_t SEASONAL_MEMBER_ROW = 4005;
    Row orphan = AbilityRow(ORPHAN_MEMBER_ROW, HERO_TREE, 1, ROW_AUTOMATIC);
    orphan.Masteries[0] = ABSENT_MASTERY;
    Add(orphan);
    Row seasonal = AbilityRow(SEASONAL_MEMBER_ROW, HERO_TREE, 1, ROW_AUTOMATIC);
    seasonal.Realms = { false, true, false, false, false };
    seasonal.Masteries[0] = MASTERY_ROW;
    Add(seasonal);
    constexpr std::uint32_t NAMING_ROW = 4006;
    Row naming = AbilityRow(NAMING_ROW, HERO_TREE, 1, 0);
    naming.Masteries[0] = MASTERY_ROW;
    Add(naming);
    catalog.Rows.at(MEMBER_ROW).TECost = 1;
    catalog.Rows.at(LATE_MEMBER_ROW).Required[0] = HERO_ROW;

    EXPECT_EQ(PriceLiveMasteries(catalog), (std::vector<std::uint32_t>{ MEMBER_ROW, LATE_MEMBER_ROW }));
    Row const& member = catalog.Rows.at(MEMBER_ROW);
    EXPECT_EQ(member.AECost, 0u);
    EXPECT_EQ(member.TECost, 0u);
    EXPECT_EQ(member.Required, (std::array<std::uint32_t, REQUIRED_COUNT>{ MASTERY_ROW, 0, 0 }));
    EXPECT_EQ(catalog.Rows.at(LATE_MEMBER_ROW).Required,
        (std::array<std::uint32_t, REQUIRED_COUNT>{ HERO_ROW, MASTERY_ROW, 0 }));
    for (std::uint32_t untouched : { ORPHAN_MEMBER_ROW, SEASONAL_MEMBER_ROW, NAMING_ROW, MASTERY_ROW })
    {
        EXPECT_EQ(catalog.Rows.at(untouched).AECost, 1u) << untouched;
        EXPECT_EQ(catalog.Rows.at(untouched).Required, (std::array<std::uint32_t, REQUIRED_COUNT>{})) << untouched;
    }
}

class RealmsLateMasteryTest : public RealmsAdvancementTest
{
protected:
    static constexpr std::uint32_t LATE_MASTERY_ROW = 4100;
    static constexpr std::uint32_t EARLY_MEMBER_ROW = 4101;
    static constexpr std::uint32_t LATER_MEMBER_ROW = 4102;

    void SetUp() override
    {
        RealmsAdvancementTest::SetUp();
        Add(AbilityRow(LATE_MASTERY_ROW, HERO_TREE, 30, ROW_MASTERY));
        for (auto const& [entryId, level] : { std::pair{ EARLY_MEMBER_ROW, 25u }, std::pair{ LATER_MEMBER_ROW, 40u } })
        {
            Row member = AbilityRow(entryId, HERO_TREE, level, ROW_AUTOMATIC);
            member.Masteries[0] = LATE_MASTERY_ROW;
            Add(member);
        }
    }
};

TEST_F(RealmsLateMasteryTest, PricingLowersAMasteryAboveItsLowestMemberToThatMembersLevel)
{
    std::vector<std::uint32_t> const priced = PriceLiveMasteries(catalog);
    EXPECT_EQ(catalog.Rows.at(LATE_MASTERY_ROW).RequiredLevel, 25u);
    EXPECT_EQ(catalog.Rows.at(MASTERY_ROW).RequiredLevel, 1u);
    EXPECT_NE(std::find(priced.begin(), priced.end(), LATE_MASTERY_ROW), priced.end());
    EXPECT_EQ(std::find(priced.begin(), priced.end(), MASTERY_ROW), priced.end());
}

TEST_F(RealmsLateMasteryTest, RepairAddsALoweredMasteryForAMemberBoughtBelowItsOldLevel)
{
    PriceLiveMasteries(catalog);
    Build build(catalog, hero, 25, { { EARLY_MEMBER_ROW, 1 } });
    EXPECT_EQ(build.RepairMembers(), 1u);
    EXPECT_TRUE(build.Has(LATE_MASTERY_ROW));
    EXPECT_TRUE(build.Has(EARLY_MEMBER_ROW));
}

class RealmsPricedMasteryTest : public RealmsAdvancementTest
{
protected:
    void SetUp() override
    {
        RealmsAdvancementTest::SetUp();
        PriceLiveMasteries(catalog);
    }
};

TEST_F(RealmsPricedMasteryTest, AMemberWithoutItsMasteryIsRefused)
{
    Build const base(catalog, hero, 10);
    EXPECT_EQ(base.ValidateLearn(MEMBER_ROW, nullptr), std::uint32_t(LEARN_MISSING_REQUIRED_ID));
    ApplyCheck const check = CheckApply(base, { { MEMBER_ROW, 1 } }, nullptr, {});
    EXPECT_EQ(check.Result, UPDATE_NOT_TRAVERSIBLE);
    EXPECT_EQ(check.Learn, std::uint32_t(LEARN_MISSING_REQUIRED_ID));
}

TEST_F(RealmsPricedMasteryTest, AMemberListedBeforeItsMasteryIsReordered)
{
    ApplyCheck const check = CheckApply(Build(catalog, hero, 10), { { MEMBER_ROW, 1 }, { MASTERY_ROW, 1 } }, nullptr,
        {});
    ASSERT_EQ(check.Result, UPDATE_OK);
    ASSERT_EQ(check.Entries.size(), 2u);
    EXPECT_EQ(check.Entries[0].EntryId, MASTERY_ROW);
    EXPECT_EQ(Build(catalog, hero, 10, check.Entries).GlobalAE(0), 1u);
}

TEST_F(RealmsPricedMasteryTest, AMasteryAutoLearnsItsMembersAtNoEssence)
{
    Build build(catalog, hero, 10, { { MASTERY_ROW, 1 } });
    EXPECT_EQ(build.AutoLearn(nullptr), 1u);
    EXPECT_TRUE(build.Has(MEMBER_ROW));
    EXPECT_EQ(catalog.Rows.at(MEMBER_ROW).AECost, 0u);
    EXPECT_EQ(build.GlobalAE(0), 1u);
}

TEST_F(RealmsPricedMasteryTest, RepairAddsTheMasteryAStoredMemberLacks)
{
    Build build(catalog, hero, 10, { { HERO_ROW, 1 }, { MEMBER_ROW, 1 } });
    EXPECT_EQ(build.RepairMembers(), 1u);
    ASSERT_EQ(build.Entries().size(), 3u);
    EXPECT_EQ(build.Entries()[1].EntryId, MASTERY_ROW);
    EXPECT_EQ(build.Entries()[2].EntryId, MEMBER_ROW);
    EXPECT_EQ(build.GlobalAE(0), 1u);
}

TEST_F(RealmsPricedMasteryTest, RepairAddsOneMasteryForTwoOfItsMembers)
{
    Build build(catalog, hero, 20, { { MEMBER_ROW, 1 }, { LATE_MEMBER_ROW, 1 } });
    EXPECT_EQ(build.RepairMembers(), 1u);
    ASSERT_EQ(build.Entries().size(), 3u);
    EXPECT_EQ(build.Entries()[0].EntryId, MASTERY_ROW);
    EXPECT_EQ(build.GlobalAE(0), 1u);
}

TEST_F(RealmsPricedMasteryTest, RestoringAStoredBuildRepairsItsMembersBeforeAutoLearning)
{
    Build build(catalog, hero, 20, { { MEMBER_ROW, 1 } });
    Restored const restored = build.Restore(nullptr);
    EXPECT_EQ(restored.Repaired, 1u);
    EXPECT_EQ(restored.Learned, 1u);
    EXPECT_TRUE(build.Has(MASTERY_ROW));
    EXPECT_TRUE(build.Has(LATE_MEMBER_ROW));
}

TEST_F(RealmsPricedMasteryTest, RepairDropsAStoredMemberWhoseMasteryDoesNotFit)
{
    Build build(catalog, hero, 19, { { MEMBER_ROW, 1 }, { LEVEL_20_SINK_ROW, 1 } });
    ASSERT_EQ(build.RemainingAE(), 0u);
    EXPECT_EQ(build.RepairMembers(), 1u);
    ASSERT_EQ(build.Entries().size(), 1u);
    EXPECT_EQ(build.Entries()[0].EntryId, LEVEL_20_SINK_ROW);
}

TEST_F(RealmsPricedMasteryTest, AGuideOmittingAHeldMasteryAppliesAndItsMembersLeave)
{
    Build const base(catalog, hero, 10, { { MASTERY_ROW, 1 }, { MEMBER_ROW, 1 } });
    std::vector<Entry> const wanted = FitGuide(catalog, hero, 10, HERO_CLASS, { { HERO_ROW, 1 } },
        GuideKeeps(catalog, base.Entries()));
    ApplyCheck const check = CheckApply(base, wanted, nullptr, {});
    ASSERT_EQ(check.Result, UPDATE_OK);
    Build built(base);
    built.SetEntries(check.Entries);
    built.AutoLearn(nullptr);
    EXPECT_TRUE(built.Has(HERO_ROW));
    EXPECT_FALSE(built.Has(MASTERY_ROW));
    EXPECT_FALSE(built.Has(MEMBER_ROW));
}

TEST_F(RealmsPricedMasteryTest, AGuideListingAMemberAloneFitsItsMasteryFirst)
{
    std::vector<Entry> const fitted = FitGuide(catalog, hero, 10, HERO_CLASS, { { MEMBER_ROW, 1 } }, {});
    ASSERT_EQ(fitted.size(), 2u);
    EXPECT_EQ(fitted[0].EntryId, MASTERY_ROW);
    EXPECT_EQ(fitted[1].EntryId, MEMBER_ROW);
    EXPECT_EQ(Build(catalog, hero, 10, fitted).GlobalAE(0), 1u);
}

class RealmsTalentPickerTest : public RealmsAdvancementTest
{
protected:
    static constexpr std::uint32_t ARMS = 6;
    static constexpr std::uint32_t PROTECTION = 7;
    static constexpr std::uint32_t TACTICS = 500;
    static constexpr std::uint32_t MORTAL_STRIKE = 501;
    static constexpr std::uint32_t ANGER = 502;
    static constexpr std::uint32_t SHIELD_WALL = 600;

    void SetUp() override
    {
        RealmsAdvancementTest::SetUp();
        Add(TalentRow(TACTICS, REBORN_WARRIOR, ARMS, 2, 10));
        Row mortalStrike = TalentRow(MORTAL_STRIKE, REBORN_WARRIOR, ARMS, 3, 10);
        mortalStrike.Required[0] = ANGER;
        Add(mortalStrike);
        Add(TalentRow(ANGER, REBORN_WARRIOR, ARMS, 1, 11));
        Add(TalentRow(SHIELD_WALL, REBORN_WARRIOR, PROTECTION, 5, 10));
    }

    Realms::TreeTabIds const tabs{ ARMS, FURY, PROTECTION };
};

TEST_F(RealmsTalentPickerTest, SpendsExactlyTheBudgetTopDownAtMaxRank)
{
    Build build(catalog, reborn, 12, {}, WARRIOR);
    EXPECT_EQ(Realms::SpendBudget(build, tabs, 0, nullptr), 3u);
    EXPECT_EQ(build.RankOf(TACTICS), 2u);
    EXPECT_EQ(build.RankOf(ANGER), 1u);
    EXPECT_EQ(build.RankOf(MORTAL_STRIKE), 0u);
    EXPECT_EQ(build.RemainingTE(), 0u);
}

TEST_F(RealmsTalentPickerTest, TakesARowOnceItsRequiredEntryIsKnown)
{
    Build build(catalog, reborn, 15, {}, WARRIOR);
    EXPECT_EQ(Realms::SpendBudget(build, tabs, 0, nullptr), 6u);
    EXPECT_EQ(build.RankOf(MORTAL_STRIKE), 3u);
    EXPECT_EQ(Realms::PointsPerTab(build, tabs), (std::map<uint8, uint32>{ { 0, 6 }, { 1, 0 }, { 2, 0 } }));
}

TEST_F(RealmsTalentPickerTest, SpillsIntoTheOtherTreesInTabOrderOnceTheChosenOneIsExhausted)
{
    Build build(catalog, reborn, 20, {}, WARRIOR);
    EXPECT_EQ(Realms::SpendBudget(build, tabs, 0, nullptr), 11u);
    EXPECT_EQ(Realms::PointsPerTab(build, tabs), (std::map<uint8, uint32>{ { 0, 6 }, { 1, 5 }, { 2, 0 } }));
}

TEST_F(RealmsTalentPickerTest, TakesARowOnceTheEntryItConnectsToIsKnown)
{
    constexpr std::uint32_t BLOOD_CRAZE = 450;
    Row bloodCraze = TalentRow(BLOOD_CRAZE, REBORN_WARRIOR, ARMS, 1, 10);
    bloodCraze.Connected[0] = ANGER;
    Add(bloodCraze);
    Build build(catalog, reborn, 13, {}, WARRIOR);
    EXPECT_EQ(build.ValidateLearn(BLOOD_CRAZE, nullptr), std::uint32_t(LEARN_MISSING_CONNECTED_ENTRIES));
    EXPECT_EQ(Realms::SpendBudget(build, tabs, 0, nullptr), 4u);
    EXPECT_EQ(build.RankOf(ANGER), 1u);
    EXPECT_EQ(build.RankOf(BLOOD_CRAZE), 1u);
}

TEST_F(RealmsTalentPickerTest, TakesARowOnceItsTreeHoldsTheTEItRequires)
{
    constexpr std::uint32_t DEEP_WOUNDS = 400;
    Row deepWounds = TalentRow(DEEP_WOUNDS, REBORN_WARRIOR, ARMS, 1, 10);
    deepWounds.TabTEInvestment = 3;
    Add(deepWounds);
    Build build(catalog, reborn, 13, {}, WARRIOR);
    EXPECT_EQ(build.ValidateLearn(DEEP_WOUNDS, nullptr), std::uint32_t(LEARN_NOT_ENOUGH_INVESTED_TE));
    EXPECT_EQ(Realms::SpendBudget(build, tabs, 0, nullptr), 4u);
    EXPECT_EQ(build.RankOf(DEEP_WOUNDS), 1u);
    EXPECT_EQ(Realms::PointsPerTab(build, tabs), (std::map<uint8, uint32>{ { 0, 4 }, { 1, 0 }, { 2, 0 } }));
}

TEST_F(RealmsTalentPickerTest, ReplacesABuildOverItsBudgetAndSpendsTheWholeBudget)
{
    Build build(catalog, reborn, 12, { { SHIELD_WALL, 5 } }, WARRIOR);
    EXPECT_EQ(Realms::SpendBudget(build, tabs, 0, nullptr), 3u);
    EXPECT_EQ(build.RankOf(SHIELD_WALL), 0u);
    EXPECT_EQ(build.RankOf(TACTICS), 2u);
    EXPECT_EQ(build.RankOf(ANGER), 1u);
    EXPECT_EQ(build.GlobalTE(0), build.TEBudget());
}

TEST(RealmsForgetTest, KeepsTheStartingKitAndForgetsTrainerQuestAndLineSpells)
{
    constexpr std::uint32_t CLASS_LINE = 11001;
    constexpr std::uint32_t OTHER_LINE = 98;
    constexpr std::uint32_t LEARNED_WITH_LINE = 100;
    constexpr std::uint32_t BOUGHT_ON_LINE = 101;
    constexpr std::uint32_t CREATE_SPELL = 102;
    constexpr std::uint32_t QUEST_REWARD = 103;
    constexpr std::uint32_t OTHER_LINE_SPELL = 104;
    constexpr std::uint32_t SOLD_AND_LEARNED_WITH_LINE = 105;
    constexpr std::uint32_t RACIAL = 106;
    std::map<std::uint32_t, std::vector<Realms::LineAbility>> const lines{
        { LEARNED_WITH_LINE, { { CLASS_LINE, 2 } } }, { BOUGHT_ON_LINE, { { CLASS_LINE, 0 } } },
        { CREATE_SPELL, { { CLASS_LINE, 0 } } }, { OTHER_LINE_SPELL, { { OTHER_LINE, 0 } } },
        { SOLD_AND_LEARNED_WITH_LINE, { { CLASS_LINE, 2 } } } };
    Realms::StartingKit const kit{ { CLASS_LINE }, { CREATE_SPELL }, { QUEST_REWARD, SOLD_AND_LEARNED_WITH_LINE } };

    std::vector<std::uint32_t> const forgotten = Realms::SpellsToForget({ LEARNED_WITH_LINE, BOUGHT_ON_LINE,
        CREATE_SPELL, QUEST_REWARD, OTHER_LINE_SPELL, SOLD_AND_LEARNED_WITH_LINE, RACIAL }, kit,
        [&lines](std::uint32_t spellId)
        {
            auto const found = lines.find(spellId);
            return found == lines.end() ? std::vector<Realms::LineAbility>{} : found->second;
        });

    EXPECT_EQ(forgotten, (std::vector<std::uint32_t>{ BOUGHT_ON_LINE, QUEST_REWARD }));
}

class RealmsStartingKitTest : public RealmsAdvancementTest
{
protected:
    static constexpr std::uint32_t REBORN_MAGE = 38;
    static constexpr std::uint32_t ARMS_LINE = 11026;
    static constexpr std::uint32_t FURY_LINE = 11256;
    static constexpr std::uint32_t STOCK_ARMS_LINE = 26;
    static constexpr std::uint32_t ARCANE_LINE = 11237;
    static constexpr std::uint32_t HEROIC_STRIKE = 1100078;
    static constexpr std::uint32_t BATTLE_SHOUT = 1106673;
    static constexpr std::uint32_t CHARGE = 1100100;
    static constexpr std::uint32_t EXECUTE = 1105308;
    static constexpr std::uint32_t PARRY = 1103127;
    static constexpr std::uint32_t DUAL_WIELD = 1100674;
    static constexpr std::uint32_t FROST_ARMOR = 1100168;

    void SetUp() override
    {
        RealmsAdvancementTest::SetUp();
        data.Catalog = catalog;
        data.Catalog.ClassTypes[REBORN_MAGE] = { MAGE, false, false, true };
        AddAbility(10, HEROIC_STRIKE, REBORN_WARRIOR, 1, 0, { ARMS_LINE, STOCK_ARMS_LINE });
        AddAbility(11, BATTLE_SHOUT, REBORN_WARRIOR, 1, 0, { FURY_LINE });
        AddAbility(12, CHARGE, REBORN_WARRIOR, 4, 0, { ARMS_LINE });
        AddAbility(13, EXECUTE, REBORN_WARRIOR, 24, 0, { FURY_LINE });
        AddAbility(14, PARRY, REBORN_WARRIOR, 6, ROW_AUTOMATIC, { ARMS_LINE });
        AddAbility(15, DUAL_WIELD, REBORN_WARRIOR, 20, ROW_AUTOMATIC, { FURY_LINE });
        AddAbility(16, FROST_ARMOR, REBORN_MAGE, 1, 0, { ARCANE_LINE });
    }

    void AddAbility(std::uint32_t entryId, std::uint32_t spellId, std::uint32_t classType, std::uint32_t level,
        std::uint32_t flags, std::vector<std::uint32_t> lines)
    {
        Row row;
        row.EntryId = entryId;
        row.Type = ENTRY_ABILITY;
        row.ClassType = classType;
        row.RequiredLevel = level;
        row.Flags = ROW_DISPLAY | flags;
        row.Realms[0] = true;
        row.Spells[0] = spellId;
        data.Catalog.RowOrder.push_back(entryId);
        data.Catalog.Rows.emplace(entryId, std::move(row));
        data.SkillLines[spellId] = std::move(lines);
    }

    Realms::StartingKit Kit(std::uint32_t startLevel, std::uint32_t level,
        std::unordered_set<std::uint32_t> trainerSpells = {}) const
    {
        return Realms::KitFrom(data, reborn, WARRIOR, startLevel, level, std::move(trainerSpells));
    }

    AscensionWarcraftReborn::Data data;
};

TEST_F(RealmsStartingKitTest, LinesAreTheRebornSkillLinesOfTheClassAbilities)
{
    EXPECT_EQ(Kit(1, 1).Lines, (std::unordered_set<std::uint32_t>{ ARMS_LINE, FURY_LINE }));
}

TEST_F(RealmsStartingKitTest, CreateSpellsHoldTheAbilitiesOfTheStartLevel)
{
    EXPECT_EQ(Kit(1, 1).CreateSpells, (std::unordered_set<std::uint32_t>{ HEROIC_STRIKE, BATTLE_SHOUT }));
    EXPECT_EQ(Kit(4, 4).CreateSpells, (std::unordered_set<std::uint32_t>{ HEROIC_STRIKE, BATTLE_SHOUT, CHARGE }));
}

TEST_F(RealmsStartingKitTest, TrainedAbilitiesAboveTheStartLevelAreNotInTheKitAtAnyLevel)
{
    Realms::StartingKit const kit = Kit(1, 30);
    EXPECT_FALSE(kit.CreateSpells.contains(CHARGE));
    EXPECT_FALSE(kit.CreateSpells.contains(EXECUTE));
}

TEST_F(RealmsStartingKitTest, CreateSpellsAddTheAutomaticAbilitiesUpToTheCharactersLevel)
{
    EXPECT_FALSE(Kit(1, 5).CreateSpells.contains(PARRY));
    Realms::StartingKit const kit = Kit(1, 19);
    EXPECT_TRUE(kit.CreateSpells.contains(PARRY));
    EXPECT_FALSE(kit.CreateSpells.contains(DUAL_WIELD));
    EXPECT_TRUE(Kit(1, 20).CreateSpells.contains(DUAL_WIELD));
}

TEST_F(RealmsStartingKitTest, TrainerSpellsAreTheTrainerSourcesGiven)
{
    EXPECT_EQ(Kit(1, 1, { CHARGE, EXECUTE }).TrainerSpells, (std::unordered_set<std::uint32_t>{ CHARGE, EXECUTE }));
}

// CharacterAdvancement.dbc's record layout as the client converts it into its 0x1FC-byte row
// (ascension-extensions-reconstruction: src/Ascension/AscDbcLayouts.generated.inc, converted by AscDbc.cpp):
// d is a DWORD, b a packed byte, L a localized string of 16 locale offsets and a flags DWORD, of which the
// client keeps the enUS offset, the first.
constexpr char ADVANCEMENT_LAYOUT[] = "ddddddddddddddddddddddddddddddddddddddddddddddd" "LdLLbbddddbddddddddddddddddd"
    "bbbbbddddddddddddddddddddddddddddddddddddddddddddddddddd";
constexpr std::array<std::uint32_t, 12> CLIENT_STRING_FIELDS = { 0x04, 0x40, 0x50, 0x60, 0xBC, 0xC0, 0xC4, 0xC8,
    0x120, 0x1A4, 0x1A8, 0x1AC };
constexpr std::uint32_t CLIENT_ROW_SIZE = 0x1FC;
constexpr std::uint32_t ADVANCEMENT_RECORD_SIZE = 692;

struct LayoutField
{
    std::uint32_t Client;
    std::uint32_t Record;
    char Kind;
};

std::vector<LayoutField> AdvancementFields()
{
    std::vector<LayoutField> fields;
    std::uint32_t client = 0;
    std::uint32_t record = 0;
    for (char kind : std::string_view(ADVANCEMENT_LAYOUT))
    {
        if (kind != 'b')
            client = (client + 3) & ~3u;
        fields.push_back({ client, record, kind });
        client += kind == 'b' ? 1 : 4;
        record += kind == 'b' ? 1 : kind == 'L' ? 68 : 4;
    }
    return fields;
}

bool IsClientString(std::uint32_t client)
{
    return std::find(CLIENT_STRING_FIELDS.begin(), CLIENT_STRING_FIELDS.end(), client) != CLIENT_STRING_FIELDS.end();
}

std::string FieldText(std::uint32_t client)
{
    return client == 0xC4 ? std::string() : "text at " + std::to_string(client);
}

struct AdvancementFile
{
    std::vector<std::uint8_t> Record = std::vector<std::uint8_t>(ADVANCEMENT_RECORD_SIZE);
    std::string Strings = std::string("string at offset zero") + '\0';

    void PutDword(std::uint32_t offset, std::uint32_t value) { std::memcpy(Record.data() + offset, &value, 4); }

    void PutString(std::uint32_t offset, std::string const& text)
    {
        PutDword(offset, std::uint32_t(Strings.size()));
        Strings += text + '\0';
    }

    std::string Write() const
    {
        std::vector<std::uint8_t> bytes = { 'W', 'D', 'B', 'C' };
        for (std::uint32_t value : { 1u, 179u, ADVANCEMENT_RECORD_SIZE, std::uint32_t(Strings.size()) })
            bytes.insert(bytes.end(), reinterpret_cast<std::uint8_t const*>(&value),
                reinterpret_cast<std::uint8_t const*>(&value) + 4);
        bytes.insert(bytes.end(), Record.begin(), Record.end());
        bytes.insert(bytes.end(), Strings.begin(), Strings.end());
        std::filesystem::path const path = std::filesystem::temp_directory_path() / ("RealmsRowPatchTest-" +
            std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + ".dbc");
        std::ofstream(path, std::ios::binary).write(reinterpret_cast<char const*>(bytes.data()), bytes.size());
        return path.string();
    }
};

AdvancementFile DistinctAdvancementRecord()
{
    AdvancementFile file;
    for (LayoutField const& field : AdvancementFields())
    {
        if (field.Kind == 'b')
            file.Record[field.Record] = std::uint8_t(field.Client);
        else if (IsClientString(field.Client))
        {
            file.PutString(field.Record, FieldText(field.Client));
            if (field.Kind == 'L')
                for (std::uint32_t locale = 1; locale < 16; ++locale)
                    file.PutString(field.Record + 4 * locale, "locale " + std::to_string(locale));
        }
        else
            file.PutDword(field.Record, 0x10000 + field.Client);
    }
    return file;
}

struct ClientRow
{
    std::array<std::uint8_t, CLIENT_ROW_SIZE> Memory{};
    std::map<std::uint32_t, std::string> Strings;
    std::size_t Consumed = 0;
    bool Overran = false;

    std::uint32_t Dword(std::uint32_t offset) const
    {
        std::uint32_t value;
        std::memcpy(&value, Memory.data() + offset, 4);
        return value;
    }
};

class PatchReader
{
public:
    explicit PatchReader(std::vector<std::uint8_t> const& bytes) : _bytes(bytes) { }

    ClientRow Read()
    {
        Dwords(0x00, 0x00);
        Text(0x04);
        Dwords(0x08, 0x3C);
        Text(0x40);
        Dwords(0x44, 0x4C);
        Text(0x50);
        Dwords(0x54, 0x5C);
        Text(0x60);
        Dwords(0x64, 0xB8);
        for (std::uint32_t offset = 0xBC; offset <= 0xC8; offset += 4)
            Text(offset);
        Byte(0xCC);
        Byte(0xCD);
        Dwords(0xD0, 0xDC);
        Byte(0xE0);
        Dwords(0xE4, 0x11C);
        Text(0x120);
        Dwords(0x124, 0x124);
        for (std::uint32_t offset = 0x128; offset <= 0x12C; ++offset)
            Byte(offset);
        Dwords(0x130, 0x1A0);
        for (std::uint32_t offset = 0x1A4; offset <= 0x1AC; offset += 4)
            Text(offset);
        Dwords(0x1B0, 0x1F8);
        _row.Consumed = _position;
        return _row;
    }

private:
    std::uint8_t Next()
    {
        if (_position >= _bytes.size())
        {
            _row.Overran = true;
            return 0;
        }
        return _bytes[_position++];
    }

    void Byte(std::uint32_t offset) { _row.Memory[offset] = Next(); }

    void Dwords(std::uint32_t first, std::uint32_t last)
    {
        for (std::uint32_t offset = first; offset <= last; offset += 4)
            for (std::uint32_t index = 0; index < 4; ++index)
                _row.Memory[offset + index] = Next();
    }

    void Text(std::uint32_t offset)
    {
        std::string text;
        for (std::uint8_t character = Next(); character && !_row.Overran; character = Next())
            text += char(character);
        _row.Strings[offset] = text;
    }

    std::vector<std::uint8_t> const& _bytes;
    std::size_t _position = 0;
    ClientRow _row;
};

TEST(RealmsRowPatchTest, TheClientLayoutSpansTheWholeRecordAndRow)
{
    LayoutField const& last = AdvancementFields().back();
    EXPECT_EQ(last.Record + 4, ADVANCEMENT_RECORD_SIZE);
    EXPECT_EQ(last.Client + 4, CLIENT_ROW_SIZE);
}

TEST(RealmsRowPatchTest, AWholeRecordReadsBackInTheClientsOrderWithOnlyItsPriceAndLevelChanged)
{
    constexpr std::uint32_t MASTERY = 1370;
    std::string const path = DistinctAdvancementRecord().Write();
    ClientDBC file;
    bool const loaded = file.Load(path, ADVANCEMENT_RECORD_SIZE / 4);
    std::filesystem::remove(path);
    ASSERT_TRUE(loaded);
    Row priced;
    priced.Required = { MASTERY, 0, 0 };
    priced.AECost = 0;
    priced.TECost = 0;
    priced.RequiredLevel = 30;

    std::vector<std::uint8_t> const bytes = RowPatch(file.GetRecord(0), priced);
    ClientRow const row = PatchReader(bytes).Read();

    ASSERT_FALSE(row.Overran);
    EXPECT_EQ(row.Consumed, bytes.size());
    std::map<std::uint32_t, std::uint32_t> const repriced = { { 0x08, MASTERY }, { 0x0C, 0 }, { 0x10, 0 },
        { 0x38, 0 }, { 0x3C, 0 }, { 0x68, 30 } };
    for (LayoutField const& field : AdvancementFields())
    {
        if (field.Kind == 'b')
            EXPECT_EQ(row.Memory[field.Client], std::uint8_t(field.Client)) << field.Client;
        else if (IsClientString(field.Client))
            EXPECT_EQ(row.Strings.at(field.Client), FieldText(field.Client)) << field.Client;
        else if (repriced.contains(field.Client))
            EXPECT_EQ(row.Dword(field.Client), repriced.at(field.Client)) << field.Client;
        else
            EXPECT_EQ(row.Dword(field.Client), 0x10000 + field.Client) << field.Client;
    }
}
}
