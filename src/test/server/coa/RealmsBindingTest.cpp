/* Copyright (C) 2016+ AzerothCore, GNU AGPL v3. */

#include "IntegrationTestFixture.h"
#include "ObjectAccessor.h"
#include "RealmsBinding.h"
#include "SpellInfoTestHelper.h"
#include <map>
#include <memory>

namespace
{
constexpr uint32 SpellStoreSize = 1200000;
constexpr uint32 Namesake = 1784;
constexpr uint32 Copy = 1101784;
constexpr uint32 ExcludedNamesake = 75;
constexpr uint32 ExcludedCopy = 1100075;
constexpr uint32 Unpaired = 2000;
constexpr uint32 MissingSpell = 1104242;

class RealmsBindingTest : public IntegrationTestFixture
{
protected:
    void SetUp() override
    {
        IntegrationTestFixture::SetUp();
        for (uint32 id : { Namesake, Copy, ExcludedNamesake, ExcludedCopy, Unpaired })
            spells[id] = SpellInfoBuilder().WithId(id).BuildUnique();
        counts = BindDefaultPairs();
    }

    void TearDown() override
    {
        Realms::ClearBinding();
        IntegrationTestFixture::TearDown();
    }

    Realms::BindingCounts BindDefaultPairs()
    {
        return Realms::Bind({ { Copy, Namesake }, { ExcludedCopy, ExcludedNamesake }, { MissingSpell, 4242 } },
            { ExcludedCopy, Namesake }, SpellStoreSize, Lookup());
    }

    Realms::SpellInfoLookup Lookup()
    {
        return [this](uint32 id)
        {
            auto spell = spells.find(id);
            return spell == spells.end() ? nullptr : spell->second.get();
        };
    }

    TestPlayer* CreatePlayerOfClass(ObjectGuid::LowType guid, uint8 classId)
    {
        TestPlayer* player = CreateTestPlayer(guid);
        player->SetByteValue(UNIT_FIELD_BYTES_0, 1, classId);
        return player;
    }

    std::map<uint32, std::unique_ptr<SpellInfo>> spells;
    Realms::BindingCounts counts;
};

bool OnlyStockEntriesHaveTemplates(uint32 entry)
{
    return entry == 2523 || entry == 1102524 || entry == 2524;
}
}

TEST_F(RealmsBindingTest, BindCountsPairsAndExclusionsAndSkipsUnknownSpells)
{
    EXPECT_EQ(counts.Pairs, 2u);
    EXPECT_EQ(counts.CorrectionExclusions, 1u);
    EXPECT_EQ(Realms::Partner(MissingSpell), 0u);
    EXPECT_TRUE(Realms::Enabled());
}

TEST_F(RealmsBindingTest, PartnerAnswersBothWaysAndZeroForAnUnpairedId)
{
    EXPECT_EQ(Realms::Partner(Namesake), Copy);
    EXPECT_EQ(Realms::Partner(Copy), Namesake);
    EXPECT_EQ(Realms::Partner(Unpaired), 0u);
    EXPECT_EQ(Realms::Partner(SpellStoreSize + 5), 0u);
}

TEST_F(RealmsBindingTest, StockIdIsTheNamesakeForACopyAndTheIdOtherwise)
{
    EXPECT_EQ(spells[Copy]->StockId(), Namesake);
    EXPECT_EQ(spells[Namesake]->StockId(), Namesake);
    EXPECT_EQ(spells[Unpaired]->StockId(), Unpaired);
    EXPECT_EQ(Realms::StockId(Copy), Namesake);
    EXPECT_EQ(Realms::StockId(Namesake), Namesake);
    EXPECT_EQ(Realms::StockId(Unpaired), Unpaired);
    EXPECT_EQ(Realms::StockId(SpellStoreSize + 5), SpellStoreSize + 5);
}

TEST_F(RealmsBindingTest, BindKeepsTheFirstPairForASpellAndSkipsSelfPairs)
{
    constexpr uint32 SecondCopy = 1102784;
    constexpr uint32 CopyOfACopy = 1102000;
    for (uint32 id : { SecondCopy, CopyOfACopy })
        spells[id] = SpellInfoBuilder().WithId(id).BuildUnique();

    Realms::BindingCounts const rebound = Realms::Bind(
        { { Copy, Namesake }, { SecondCopy, Namesake }, { Unpaired, Unpaired }, { CopyOfACopy, Copy } }, {},
        SpellStoreSize, Lookup());

    EXPECT_EQ(rebound.Pairs, 1u);
    EXPECT_EQ(Realms::Partner(Namesake), Copy);
    EXPECT_EQ(Realms::Partner(Copy), Namesake);
    EXPECT_EQ(Realms::Partner(SecondCopy), 0u);
    EXPECT_EQ(spells[SecondCopy]->RealmsNamesake, 0u);
    EXPECT_EQ(Realms::Partner(Unpaired), 0u);
    EXPECT_EQ(Realms::Partner(CopyOfACopy), 0u);
}

TEST_F(RealmsBindingTest, CorrectionExclusionNamesOnlyListedCopies)
{
    EXPECT_TRUE(Realms::CorrectionExcluded(ExcludedCopy));
    EXPECT_FALSE(Realms::CorrectionExcluded(Copy));
    EXPECT_FALSE(Realms::CorrectionExcluded(Namesake));
}

TEST_F(RealmsBindingTest, ForCasterResolvesANamesakeToItsCopyForAStockClassPlayer)
{
    TestPlayer* warrior = CreatePlayerOfClass(1, CLASS_WARRIOR);
    TestPlayer* druid = CreatePlayerOfClass(2, CLASS_DRUID);
    EXPECT_EQ(Realms::ForCaster(warrior, Namesake), Copy);
    EXPECT_EQ(Realms::ForCaster(druid, Namesake), Copy);
    EXPECT_EQ(Realms::ForCaster(warrior, Copy), Copy);
    EXPECT_EQ(Realms::ForCaster(warrior, Unpaired), Unpaired);
    EXPECT_EQ(Realms::ForPlayer(warrior, Namesake), Copy);
}

TEST_F(RealmsBindingTest, ForCasterResolvesForAUnitOwnedByAStockClassPlayer)
{
    TestPlayer* hunter = CreatePlayerOfClass(3, CLASS_HUNTER);
    ObjectAccessor::AddObject<Player>(hunter);
    TestCreature* pet = CreateTestCreature(10, 510, TEST_FACTION_HOSTILE_TO_MONSTERS);
    pet->SetOwnerGUID(hunter->GetGUID());

    EXPECT_EQ(Realms::ForCaster(pet, Namesake), Copy);

    ObjectAccessor::RemoveObject<Player>(hunter);
}

TEST_F(RealmsBindingTest, ForCasterKeepsTheIdForCoAClassesHeroesAndNpcs)
{
    TestPlayer* barbarian = CreatePlayerOfClass(4, CLASS_BARBARIAN);
    TestPlayer* hero = CreatePlayerOfClass(5, CLASS_HERO);
    TestCreature* npc = CreateTestCreature(11, 511, TEST_FACTION_HOSTILE_TO_ALL);

    EXPECT_EQ(Realms::ForCaster(barbarian, Namesake), Namesake);
    EXPECT_EQ(Realms::ForCaster(hero, Namesake), Namesake);
    EXPECT_EQ(Realms::ForCaster(npc, Namesake), Namesake);
    EXPECT_EQ(Realms::ForCaster(nullptr, Namesake), Namesake);
    EXPECT_EQ(Realms::ForPlayer(nullptr, Namesake), Namesake);
}

TEST_F(RealmsBindingTest, AnEmptyMapResolvesNothing)
{
    Realms::ClearBinding();
    TestPlayer* warrior = CreatePlayerOfClass(6, CLASS_WARRIOR);

    EXPECT_FALSE(Realms::Enabled());
    EXPECT_EQ(Realms::ForCaster(warrior, Namesake), Namesake);
    EXPECT_EQ(Realms::Partner(Copy), 0u);
    EXPECT_EQ(Realms::CreatureEntry(1102523, OnlyStockEntriesHaveTemplates), 1102523u);
}

TEST_F(RealmsBindingTest, CreatureEntryRedirectsOnlyAMissingTemplateAboveTheCopyRange)
{
    EXPECT_EQ(Realms::CreatureEntry(1102523, OnlyStockEntriesHaveTemplates), 2523u);
    EXPECT_EQ(Realms::CreatureEntry(1102524, OnlyStockEntriesHaveTemplates), 1102524u);
    EXPECT_EQ(Realms::CreatureEntry(1102525, OnlyStockEntriesHaveTemplates), 1102525u);
    EXPECT_EQ(Realms::CreatureEntry(2523, OnlyStockEntriesHaveTemplates), 2523u);
}
