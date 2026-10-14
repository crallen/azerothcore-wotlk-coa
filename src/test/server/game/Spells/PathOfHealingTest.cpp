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
#include "SpellAuras.h"
#include "SpellInfoTestHelper.h"
#include "gtest/gtest.h"

namespace
{
constexpr uint32 SPELL_PATH_OF_HEALING = 84867;

class PathOfHealingTest : public IntegrationTestFixture
{
protected:
    TestPlayer* CreatePlayerWithSpellPower(int32 spellPower)
    {
        TestPlayer* player = CreateTestPlayer();
        player->ApplySpellPowerBonus(spellPower, true);
        return player;
    }

    std::unique_ptr<SpellInfo> CreatePathOfHealingAura()
    {
        return SpellInfoBuilder()
            .WithId(SPELL_PATH_OF_HEALING)
            .WithEffect(0, SPELL_EFFECT_APPLY_AURA, SPELL_AURA_DUMMY)
            .BuildUnique();
    }
};

TEST_F(PathOfHealingTest, WithoutThePathSpellPowerCountsOnceForHealingAndDamage)
{
    TestPlayer* player = CreatePlayerWithSpellPower(100);

    EXPECT_EQ(player->SpellBaseHealingBonusDone(SPELL_SCHOOL_MASK_HOLY), 100);
    EXPECT_EQ(player->SpellBaseDamageBonusDone(SPELL_SCHOOL_MASK_HOLY), 100);
}

TEST_F(PathOfHealingTest, ThePathDoublesSpellPowerForHealingOnly)
{
    TestPlayer* player = CreatePlayerWithSpellPower(100);
    auto spell = CreatePathOfHealingAura();

    ASSERT_NE(player->AddAura(spell.get(), 1, player), nullptr);

    EXPECT_EQ(player->SpellBaseHealingBonusDone(SPELL_SCHOOL_MASK_HOLY), 200);
    EXPECT_EQ(player->SpellBaseDamageBonusDone(SPELL_SCHOOL_MASK_HOLY), 100);
}

TEST_F(PathOfHealingTest, SheetBonusHealingFollowsThePathOnAndOff)
{
    TestPlayer* player = CreatePlayerWithSpellPower(100);
    auto spell = CreatePathOfHealingAura();

    Aura* aura = player->AddAura(spell.get(), 1, player);
    ASSERT_NE(aura, nullptr);

    EXPECT_EQ(player->GetInt32Value(PLAYER_FIELD_MOD_HEALING_DONE_POS), 200);
    EXPECT_EQ(player->GetInt32Value(PLAYER_FIELD_MOD_DAMAGE_DONE_POS + SPELL_SCHOOL_HOLY), 100);

    aura->Remove();

    EXPECT_EQ(player->GetInt32Value(PLAYER_FIELD_MOD_HEALING_DONE_POS), 100);
}
}
