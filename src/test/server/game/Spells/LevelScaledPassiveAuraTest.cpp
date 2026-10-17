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

#include "IntegrationTestFixture.h"
#include "SpellAuraEffects.h"
#include "SpellAuras.h"
#include "SpellInfoTestHelper.h"
#include "SpellMgr.h"
#include "gtest/gtest.h"

namespace
{
constexpr uint32 SPELL_LEVEL_SCALED_STRENGTH = 990043;
constexpr uint32 SPELL_LINKING_PASSIVE = 990044;

// Path of Strength's flat Strength (820043 effect 1): MOD_STAT Strength, base points 3,
// die sides 1, 0.5 per level, so 4 + level / 2 (9 at level 10, 14 at level 20).
class LevelScaledPassiveAuraTest : public IntegrationTestFixture
{
protected:
    void TearDown() override
    {
        IntegrationTestFixture::TearDown();
        sSpellMgr->SetSpellLinkedForTest(int32(SPELL_LINKING_PASSIVE) + SPELL_LINK_AURA, {});
        sSpellMgr->UnloadSpellInfoStore();
    }

    TestPlayer* CreatePlayerAtLevel(uint8 level)
    {
        TestPlayer* player = CreateTestPlayer(++_guid);
        player->SetLevel(level);
        return player;
    }

    static std::unique_ptr<SpellInfo> CreateLevelScaledStrengthAura(bool passive)
    {
        std::unique_ptr<SpellInfo> spell = SpellInfoBuilder()
            .WithId(SPELL_LEVEL_SCALED_STRENGTH)
            .WithAttributes(passive ? SPELL_ATTR0_PASSIVE : 0)
            .WithEffect(0, SPELL_EFFECT_APPLY_AURA, SPELL_AURA_MOD_STAT)
            .WithEffectMiscValue(0, STAT_STRENGTH)
            .WithEffectBasePoints(0, 3)
            .WithEffectDieSides(0, 1)
            .BuildUnique();
        spell->Effects[0].RealPointsPerLevel = 0.5f;
        return spell;
    }

private:
    ObjectGuid::LowType _guid = 0;
};

TEST_F(LevelScaledPassiveAuraTest, ALevelUpRescalesAPassivesPerLevelAmount)
{
    TestPlayer* player = CreatePlayerAtLevel(10);
    auto spell = CreateLevelScaledStrengthAura(true);

    Aura* aura = player->AddAura(spell.get(), MAX_EFFECT_MASK, player);
    ASSERT_NE(aura, nullptr);
    EXPECT_EQ(aura->GetEffect(0)->GetAmount(), 9);

    player->SetLevel(20);
    EXPECT_EQ(aura->GetEffect(0)->GetAmount(), 9);

    player->RecalculateLevelScaledPassiveAuras();
    EXPECT_EQ(aura->GetEffect(0)->GetAmount(), 14);
}

// Path of Agility's ranged attack power (954688) is not passive; the passive 84865 applies it
// through a spell_linked_spell aura link.
TEST_F(LevelScaledPassiveAuraTest, ALevelUpRescalesAnAuraLinkedFromAPassive)
{
    TestPlayer* player = CreatePlayerAtLevel(10);
    sSpellMgr->AddSpellInfoForTest(SPELL_LEVEL_SCALED_STRENGTH, CreateLevelScaledStrengthAura(false).release());
    sSpellMgr->AddSpellInfoForTest(SPELL_LINKING_PASSIVE, SpellInfoBuilder()
        .WithId(SPELL_LINKING_PASSIVE)
        .WithAttributes(SPELL_ATTR0_PASSIVE)
        .WithEffect(0, SPELL_EFFECT_APPLY_AURA, SPELL_AURA_DUMMY)
        .Build());
    sSpellMgr->SetSpellLinkedForTest(int32(SPELL_LINKING_PASSIVE) + SPELL_LINK_AURA, { int32(SPELL_LEVEL_SCALED_STRENGTH) });

    ASSERT_NE(player->AddAura(SPELL_LINKING_PASSIVE, player), nullptr);
    Aura* linked = player->GetAura(SPELL_LEVEL_SCALED_STRENGTH);
    ASSERT_NE(linked, nullptr);
    EXPECT_EQ(linked->GetEffect(0)->GetAmount(), 9);

    player->SetLevel(20);
    player->RecalculateLevelScaledPassiveAuras();

    EXPECT_EQ(linked->GetEffect(0)->GetAmount(), 14);
}

TEST_F(LevelScaledPassiveAuraTest, LeavesNonPassiveAurasAlone)
{
    TestPlayer* player = CreatePlayerAtLevel(10);
    auto spell = CreateLevelScaledStrengthAura(false);

    Aura* aura = player->AddAura(spell.get(), MAX_EFFECT_MASK, player);
    ASSERT_NE(aura, nullptr);

    player->SetLevel(20);
    player->RecalculateLevelScaledPassiveAuras();

    EXPECT_EQ(aura->GetEffect(0)->GetAmount(), 9);
}

TEST_F(LevelScaledPassiveAuraTest, LeavesEffectsWithoutAPerLevelPartAlone)
{
    TestPlayer* player = CreatePlayerAtLevel(10);
    std::unique_ptr<SpellInfo> spell = SpellInfoBuilder()
        .WithId(SPELL_LEVEL_SCALED_STRENGTH)
        .WithAttributes(SPELL_ATTR0_PASSIVE)
        .WithEffect(0, SPELL_EFFECT_APPLY_AURA, SPELL_AURA_MOD_STAT)
        .WithEffectMiscValue(0, STAT_STRENGTH)
        .WithEffectBasePoints(0, 3)
        .WithEffectDieSides(0, 1)
        .WithEffect(1, SPELL_EFFECT_APPLY_AURA, SPELL_AURA_MOD_STAT)
        .WithEffectMiscValue(1, STAT_AGILITY)
        .WithEffectBasePoints(1, 9)
        .WithEffectDieSides(1, 1)
        .BuildUnique();
    spell->Effects[0].RealPointsPerLevel = 0.5f;

    Aura* aura = player->AddAura(spell.get(), MAX_EFFECT_MASK, player);
    ASSERT_NE(aura, nullptr);
    ASSERT_NE(aura->GetEffect(1), nullptr);
    // Unmarked, so the effect stays recalculable and only the per-level check keeps it at 50.
    aura->GetEffect(1)->ChangeAmount(50, false);

    player->SetLevel(20);
    player->RecalculateLevelScaledPassiveAuras();

    EXPECT_EQ(aura->GetEffect(0)->GetAmount(), 14);
    EXPECT_EQ(aura->GetEffect(1)->GetAmount(), 50);
}

TEST_F(LevelScaledPassiveAuraTest, LeavesAurasCastByOthersAlone)
{
    TestPlayer* player = CreatePlayerAtLevel(10);
    TestPlayer* otherPlayer = CreatePlayerAtLevel(10);
    auto spell = CreateLevelScaledStrengthAura(true);

    Aura* aura = otherPlayer->AddAura(spell.get(), MAX_EFFECT_MASK, player);
    ASSERT_NE(aura, nullptr);
    ASSERT_EQ(aura->GetOwner(), player);

    player->SetLevel(20);
    player->RecalculateLevelScaledPassiveAuras();

    EXPECT_EQ(aura->GetEffect(0)->GetAmount(), 9);
}
}
