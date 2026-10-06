/* Copyright (C) 2016+ AzerothCore, GNU AGPL v3. */

#include "IntegrationTestFixture.h"
#include "ObjectAccessor.h"
#include "RealmsBinding.h"
#include "SpellInfoTestHelper.h"
#include "SpellMgr.h"
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

TEST_F(RealmsBindingTest, TwinSourceIsThePairListNamesakeForACopyWithNoTwin)
{
    constexpr uint32 RenamedNamesake = 50622;
    constexpr uint32 RenamedCopy = 1150622;
    for (uint32 id : { RenamedNamesake, RenamedCopy })
        spells[id] = SpellInfoBuilder().WithId(id).BuildUnique();
    Realms::Bind({ { RenamedCopy, RenamedNamesake } }, {}, SpellStoreSize, Lookup());

    EXPECT_EQ(sSpellMgr->GetSpellTwinSource(RenamedCopy), RenamedNamesake);
    EXPECT_EQ(sSpellMgr->GetSpellTwinSource(RenamedNamesake), RenamedNamesake);
    EXPECT_EQ(sSpellMgr->GetSpellTwinSource(Unpaired), Unpaired);
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
}

TEST_F(RealmsBindingTest, MirrorCorrectionAppliesANamesakesFixToItsCopyAndCountsIt)
{
    Realms::SpellFix const fix = [](SpellInfo* spell) { spell->AttributesEx3 |= 0x40; };

    Realms::MirrorCorrection(Namesake, fix, {});

    EXPECT_EQ(spells[Copy]->AttributesEx3, 0x40u);
    EXPECT_EQ(spells[Namesake]->AttributesEx3, 0u);
    EXPECT_EQ(Realms::ReportBinding().CorrectionsMirrored, 1u);
}

TEST_F(RealmsBindingTest, MirrorCorrectionLeavesACopyTheTwinPassCorrectedAndCountsNothing)
{
    Realms::SpellFix const fix = [](SpellInfo* spell) { spell->AttributesEx3 |= 0x40; };

    Realms::MirrorCorrection(Namesake, fix, { spells[Copy].get() });

    EXPECT_EQ(spells[Copy]->AttributesEx3, 0u);
    EXPECT_EQ(Realms::ReportBinding().CorrectionsMirrored, 0u);
}

TEST_F(RealmsBindingTest, MirrorCorrectionSkipsExcludedCopiesCopiesAndUnpairedIds)
{
    Realms::SpellFix const fix = [](SpellInfo* spell) { spell->AttributesEx3 |= 0x40; };

    Realms::MirrorCorrection(ExcludedNamesake, fix, {});
    Realms::MirrorCorrection(Copy, fix, {});
    Realms::MirrorCorrection(Unpaired, fix, {});

    EXPECT_EQ(spells[ExcludedCopy]->AttributesEx3, 0u);
    EXPECT_EQ(spells[Namesake]->AttributesEx3, 0u);
    EXPECT_EQ(spells[Copy]->AttributesEx3, 0u);
    EXPECT_EQ(spells[Unpaired]->AttributesEx3, 0u);
    EXPECT_EQ(Realms::ReportBinding().CorrectionsMirrored, 0u);
}

TEST_F(RealmsBindingTest, BindGivesACopyWithoutProcFlagsItsNamesakesAndKeepsItsOwn)
{
    constexpr uint32 SealNamesake = 21084;
    constexpr uint32 SealCopy = 1121084;
    constexpr uint32 OwnFlagsNamesake = 20154;
    constexpr uint32 OwnFlagsCopy = 1120154;
    spells[SealNamesake] = SpellInfoBuilder().WithId(SealNamesake).WithProcFlags(20).WithProcChance(100).BuildUnique();
    spells[SealCopy] = SpellInfoBuilder().WithId(SealCopy).WithProcChance(101).BuildUnique();
    spells[OwnFlagsNamesake] = SpellInfoBuilder().WithId(OwnFlagsNamesake).WithProcFlags(4).BuildUnique();
    spells[OwnFlagsCopy] = SpellInfoBuilder().WithId(OwnFlagsCopy).WithProcFlags(20).BuildUnique();

    Realms::BindingCounts const bound = Realms::Bind(
        { { SealCopy, SealNamesake }, { OwnFlagsCopy, OwnFlagsNamesake }, { Copy, Namesake } },
        {}, SpellStoreSize, Lookup());

    EXPECT_EQ(spells[SealCopy]->ProcFlags, 20u);
    EXPECT_EQ(spells[SealCopy]->ProcChance, 101u);
    EXPECT_EQ(spells[OwnFlagsCopy]->ProcFlags, 20u);
    EXPECT_EQ(spells[Copy]->ProcFlags, 0u);
    EXPECT_EQ(bound.ProcFlagsMirrored, 1u);
}

TEST_F(RealmsBindingTest, BindFillsAZeroProcChanceAndFillsChargesOnlyWhereTheCopyProcs)
{
    constexpr uint32 ShockNamesake = 51525;
    constexpr uint32 ShockCopy = 1151525;
    constexpr uint32 MendingNamesake = 41635;
    constexpr uint32 MendingCopy = 1141635;
    spells[ShockNamesake] = SpellInfoBuilder().WithId(ShockNamesake).WithProcFlags(4).WithProcChance(5)
        .WithProcCharges(2).BuildUnique();
    spells[ShockCopy] = SpellInfoBuilder().WithId(ShockCopy).WithProcFlags(4).WithProcCharges(3).BuildUnique();
    spells[MendingNamesake] = SpellInfoBuilder().WithId(MendingNamesake).WithProcFlags(0x20).WithProcChance(100)
        .WithProcCharges(5).BuildUnique();
    spells[MendingCopy] = SpellInfoBuilder().WithId(MendingCopy).WithProcFlags(0x20).WithProcChance(100).BuildUnique();
    spells[Namesake]->ProcChance = 50;
    spells[Namesake]->ProcCharges = 1;

    Realms::BindingCounts const bound = Realms::Bind(
        { { ShockCopy, ShockNamesake }, { MendingCopy, MendingNamesake }, { Copy, Namesake } },
        {}, SpellStoreSize, Lookup());

    EXPECT_EQ(spells[ShockCopy]->ProcChance, 5u);
    EXPECT_EQ(spells[ShockCopy]->ProcCharges, 3u);
    EXPECT_EQ(spells[MendingCopy]->ProcCharges, 5u);
    EXPECT_EQ(spells[Copy]->ProcChance, 50u);
    EXPECT_EQ(spells[Copy]->ProcCharges, 0u);
    EXPECT_EQ(bound.ProcChancesMirrored, 2u);
    EXPECT_EQ(bound.ProcChargesMirrored, 1u);
}

TEST_F(RealmsBindingTest, BindFillsAZeroClassMaskUnderTheSameAuraAndAddsTheNamesakesFamilyFlags)
{
    constexpr uint32 LightNamesake = 20359;
    constexpr uint32 LightCopy = 1120359;
    constexpr uint32 SufferingNamesake = 47581;
    constexpr uint32 SufferingCopy = 1147581;
    constexpr uint32 OwnMaskNamesake = 62097;
    constexpr uint32 OwnMaskCopy = 1162097;
    auto modifier = [](uint32 id, uint32 family)
    {
        return SpellInfoBuilder().WithId(id).WithSpellFamilyName(family)
            .WithEffect(EFFECT_0, SPELL_EFFECT_APPLY_AURA, SPELL_AURA_ADD_PCT_MODIFIER).BuildUnique();
    };
    spells[LightNamesake] = modifier(LightNamesake, SPELLFAMILY_PALADIN);
    spells[LightNamesake]->_GetEffect(EFFECT_0).SpellClassMask = flag96(0x80200000, 0, 0);
    spells[LightCopy] = modifier(LightCopy, SPELLFAMILY_PALADIN);
    spells[OwnMaskNamesake] = modifier(OwnMaskNamesake, SPELLFAMILY_SHAMAN);
    spells[OwnMaskNamesake]->_GetEffect(EFFECT_0).SpellClassMask = flag96(0x1, 0, 0x2);
    spells[OwnMaskCopy] = modifier(OwnMaskCopy, SPELLFAMILY_SHAMAN);
    spells[OwnMaskCopy]->_GetEffect(EFFECT_0).SpellClassMask = flag96(0x2, 0x1, 0x2);
    spells[SufferingNamesake] = SpellInfoBuilder().WithId(SufferingNamesake).WithSpellFamilyName(SPELLFAMILY_PRIEST)
        .WithSpellFamilyFlags(0x800000).BuildUnique();
    spells[SufferingCopy] = SpellInfoBuilder().WithId(SufferingCopy).WithSpellFamilyName(SPELLFAMILY_PRIEST)
        .WithSpellFamilyFlags(0, 0x10).BuildUnique();

    Realms::BindingCounts const bound = Realms::Bind({ { LightCopy, LightNamesake },
        { SufferingCopy, SufferingNamesake }, { OwnMaskCopy, OwnMaskNamesake } }, {}, SpellStoreSize, Lookup());

    EXPECT_EQ(spells[LightCopy]->GetEffect(EFFECT_0).SpellClassMask, flag96(0x80200000, 0, 0));
    EXPECT_EQ(spells[OwnMaskCopy]->GetEffect(EFFECT_0).SpellClassMask, flag96(0x2, 0x1, 0x2));
    EXPECT_EQ(spells[SufferingCopy]->SpellFamilyFlags, flag96(0x800000, 0x10, 0));
    EXPECT_EQ(bound.ClassMasksMirrored, 1u);
    EXPECT_EQ(bound.FamilyFlagsMirrored, 1u);
}

TEST_F(RealmsBindingTest, BindReplacesOnlyATriggerSpellTheStoreLacks)
{
    constexpr uint32 InvisibilityNamesake = 66;
    constexpr uint32 InvisibilityCopy = 1100066;
    constexpr uint32 Fade = 32612;
    constexpr uint32 MissingFade = 1135009;
    spells[Fade] = SpellInfoBuilder().WithId(Fade).BuildUnique();
    spells[InvisibilityNamesake] = SpellInfoBuilder().WithId(InvisibilityNamesake)
        .WithEffect(EFFECT_1, SPELL_EFFECT_APPLY_AURA, SPELL_AURA_PERIODIC_TRIGGER_SPELL)
        .WithEffectTriggerSpell(EFFECT_1, Fade).BuildUnique();
    spells[InvisibilityCopy] = SpellInfoBuilder().WithId(InvisibilityCopy)
        .WithEffect(EFFECT_1, SPELL_EFFECT_APPLY_AURA, SPELL_AURA_PERIODIC_TRIGGER_SPELL)
        .WithEffectTriggerSpell(EFFECT_1, MissingFade).BuildUnique();
    spells[Namesake] = SpellInfoBuilder().WithId(Namesake).WithEffectTriggerSpell(EFFECT_0, Fade).BuildUnique();
    spells[Copy] = SpellInfoBuilder().WithId(Copy).WithEffectTriggerSpell(EFFECT_0, Unpaired).BuildUnique();

    Realms::BindingCounts const bound = Realms::Bind(
        { { InvisibilityCopy, InvisibilityNamesake }, { Copy, Namesake } }, {}, SpellStoreSize, Lookup());

    EXPECT_EQ(spells[InvisibilityCopy]->GetEffect(EFFECT_1).TriggerSpell, Fade);
    EXPECT_EQ(spells[Copy]->GetEffect(EFFECT_0).TriggerSpell, Unpaired);
    EXPECT_EQ(bound.TriggersMirrored, 1u);
}

TEST_F(RealmsBindingTest, ReportBindingCarriesTheLoadedCountsAndNothingWhenEmpty)
{
    Realms::BindingCounts const report = Realms::ReportBinding();
    EXPECT_EQ(report.Pairs, 2u);
    EXPECT_EQ(report.CorrectionExclusions, 1u);

    Realms::ClearBinding();
    EXPECT_EQ(Realms::ReportBinding().Pairs, 0u);
}
