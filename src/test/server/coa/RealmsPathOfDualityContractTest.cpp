/* Copyright (C) 2016+ AzerothCore, GNU AGPL v3. */

// wow-realms: Path of Duality's flat Intellect, filled into the effect slot its DBC record leaves empty.

#include "AscensionFreepick.h"
#include "SpellInfoTestHelper.h"
#include "gtest/gtest.h"

namespace
{
constexpr uint32 PathOfDuality = 820054;

// 820054 as Spell.dbc ships it: flat attack power, an empty slot, and a dummy on the caster.
SpellInfoBuilder PathOfDualityRecord()
{
    return SpellInfoBuilder()
        .WithId(PathOfDuality)
        .WithEffect(0, SPELL_EFFECT_APPLY_AURA, SPELL_AURA_ASCENSION_MOD_ATTACK_POWER_FLAT)
        .WithEffectBasePoints(0, -1)
        .WithEffectDieSides(0, 1)
        .WithEffect(2, SPELL_EFFECT_APPLY_AURA, SPELL_AURA_DUMMY);
}
}

TEST(RealmsPathOfDualityContractTest, FillsTheEmptyIntellectSlot)
{
    std::unique_ptr<SpellInfo> info = PathOfDualityRecord().BuildUnique();
    info->Effects[EFFECT_2].TargetA = SpellImplicitTargetInfo(TARGET_UNIT_CASTER);

    ApplyAscensionPathPassiveContract(info.get());

    SpellEffectInfo const& intellect = info->Effects[EFFECT_1];
    EXPECT_EQ(intellect.Effect, uint32(SPELL_EFFECT_APPLY_AURA));
    EXPECT_EQ(intellect.ApplyAuraName, SPELL_AURA_MOD_STAT);
    EXPECT_EQ(intellect.MiscValue, int32(STAT_INTELLECT));
    EXPECT_EQ(intellect.BasePoints, 3);
    EXPECT_EQ(intellect.DieSides, 1);
    EXPECT_FLOAT_EQ(intellect.RealPointsPerLevel, 0.5f);
    EXPECT_EQ(intellect.TargetA.GetTarget(), TARGET_UNIT_CASTER);
}

TEST(RealmsPathOfDualityContractTest, SkipsAnOccupiedSlot)
{
    std::unique_ptr<SpellInfo> info = PathOfDualityRecord()
                                          .WithEffect(1, SPELL_EFFECT_APPLY_AURA, SPELL_AURA_MOD_STAT)
                                          .WithEffectMiscValue(1, STAT_STRENGTH)
                                          .WithEffectBasePoints(1, 7)
                                          .BuildUnique();

    ApplyAscensionPathPassiveContract(info.get());

    SpellEffectInfo const& occupied = info->Effects[EFFECT_1];
    EXPECT_EQ(occupied.Effect, uint32(SPELL_EFFECT_APPLY_AURA));
    EXPECT_EQ(occupied.ApplyAuraName, SPELL_AURA_MOD_STAT);
    EXPECT_EQ(occupied.MiscValue, int32(STAT_STRENGTH));
    EXPECT_EQ(occupied.BasePoints, 7);
    EXPECT_EQ(occupied.DieSides, 0);
    EXPECT_FLOAT_EQ(occupied.RealPointsPerLevel, 0.0f);
}
