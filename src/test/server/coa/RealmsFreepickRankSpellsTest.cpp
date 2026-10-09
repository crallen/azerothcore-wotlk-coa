/* Copyright (C) 2016+ AzerothCore, GNU AGPL v3. */

// wow-realms: the spells a free-pick talent rank grants, read through the SpellMgr the server loads.

#include "AscensionFreepick.h"
#include "SpellInfoTestHelper.h"
#include "SpellMgr.h"
#include "gtest/gtest.h"

namespace
{
// The Warcraft Reborn copy of Mangle teaches the two ranked Mangle spells a trainer ranks up,
// which the SpellMgr marks as additional talent spells.
constexpr uint32 RebornMangle = 1133917;
constexpr uint32 MangleBear = 1133878;
constexpr uint32 MangleCat = 1133876;

class RealmsFreepickRankSpellsTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        sSpellMgr->AddSpellInfoForTest(RebornMangle, SpellInfoBuilder()
                                                         .WithId(RebornMangle)
                                                         .WithEffect(0, SPELL_EFFECT_LEARN_SPELL)
                                                         .WithEffectTriggerSpell(0, MangleBear)
                                                         .WithEffect(1, SPELL_EFFECT_LEARN_SPELL)
                                                         .WithEffectTriggerSpell(1, MangleCat)
                                                         .Build());
        for (uint32 id : { MangleBear, MangleCat })
        {
            sSpellMgr->AddSpellInfoForTest(id, SpellInfoBuilder().WithId(id).Build());
            sSpellMgr->SetAdditionalTalentSpellForTest(id, true);
        }
    }

    void TearDown() override
    {
        for (uint32 id : { MangleBear, MangleCat })
            sSpellMgr->SetAdditionalTalentSpellForTest(id, false);
        sSpellMgr->UnloadSpellInfoStore();
    }
};
}

TEST_F(RealmsFreepickRankSpellsTest, RankTeachingAdditionalTalentSpellsGrantsThem)
{
    EXPECT_EQ(AscensionFreepick::RankSpells(RebornMangle), std::vector<std::uint32_t>({ MangleBear, MangleCat }));
}
