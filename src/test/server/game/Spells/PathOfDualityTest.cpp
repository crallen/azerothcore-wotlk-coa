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
class PathOfDualityTest : public IntegrationTestFixture
{
protected:
    TestPlayer* CreatePlayerWithStats(int32 agility, int32 intellect)
    {
        TestPlayer* player = CreateTestPlayer(++_guid);
        player->SetStat(STAT_AGILITY, agility);
        player->SetStat(STAT_INTELLECT, intellect);
        return player;
    }

    static std::unique_ptr<SpellInfo> CreatePathOfDualityStatBuffAura()
    {
        return SpellInfoBuilder()
            .WithId(SPELL_PATH_OF_DUALITY_STAT_BUFF)
            .WithEffect(0, SPELL_EFFECT_APPLY_AURA, SPELL_AURA_DUMMY)
            .BuildUnique();
    }

private:
    ObjectGuid::LowType _guid = 0;
};

TEST_F(PathOfDualityTest, CritStatIsItsOwnStatWithoutThePath)
{
    TestPlayer* player = CreatePlayerWithStats(100, 300);

    EXPECT_FLOAT_EQ(player->GetCritStat(STAT_AGILITY), 100.0f);
    EXPECT_FLOAT_EQ(player->GetCritStat(STAT_INTELLECT), 300.0f);
}

TEST_F(PathOfDualityTest, CritStatIsTheHigherOfAgilityAndIntellectOnThePath)
{
    auto spell = CreatePathOfDualityStatBuffAura();

    TestPlayer* intellectHeavy = CreatePlayerWithStats(100, 300);
    intellectHeavy->SetStat(STAT_STRENGTH, 50);
    ASSERT_NE(intellectHeavy->AddAura(spell.get(), 1, intellectHeavy), nullptr);

    EXPECT_FLOAT_EQ(intellectHeavy->GetCritStat(STAT_AGILITY), 300.0f);
    EXPECT_FLOAT_EQ(intellectHeavy->GetCritStat(STAT_INTELLECT), 300.0f);
    EXPECT_FLOAT_EQ(intellectHeavy->GetCritStat(STAT_STRENGTH), 50.0f);

    TestPlayer* agilityHeavy = CreatePlayerWithStats(300, 100);
    ASSERT_NE(agilityHeavy->AddAura(spell.get(), 1, agilityHeavy), nullptr);

    EXPECT_FLOAT_EQ(agilityHeavy->GetCritStat(STAT_AGILITY), 300.0f);
    EXPECT_FLOAT_EQ(agilityHeavy->GetCritStat(STAT_INTELLECT), 300.0f);
}

TEST_F(PathOfDualityTest, IntellectAddsThirtyPercentSpellPowerAfterTheMultiplier)
{
    TestPlayer* player = CreatePlayerWithStats(0, 200);
    player->ApplySpellPowerBonus(100, true);

    EXPECT_EQ(player->SpellBaseDamageBonusDone(SPELL_SCHOOL_MASK_HOLY), 100);
    EXPECT_EQ(player->SpellBaseHealingBonusDone(SPELL_SCHOOL_MASK_HOLY), 100);

    auto spell = CreatePathOfDualityStatBuffAura();
    ASSERT_NE(player->AddAura(spell.get(), 1, player), nullptr);

    // 100 x 1.75 + 30% of 200 Intellect
    EXPECT_EQ(player->SpellBaseDamageBonusDone(SPELL_SCHOOL_MASK_HOLY), 235);
    EXPECT_EQ(player->SpellBaseHealingBonusDone(SPELL_SCHOOL_MASK_HOLY), 235);
}

TEST_F(PathOfDualityTest, SheetSpellPowerFollowsThePathOnAndOff)
{
    TestPlayer* player = CreatePlayerWithStats(0, 200);
    player->ApplySpellPowerBonus(100, true);
    auto spell = CreatePathOfDualityStatBuffAura();

    Aura* aura = player->AddAura(spell.get(), 1, player);
    ASSERT_NE(aura, nullptr);

    EXPECT_EQ(player->GetInt32Value(PLAYER_FIELD_MOD_DAMAGE_DONE_POS + SPELL_SCHOOL_HOLY), 235);
    EXPECT_EQ(player->GetInt32Value(PLAYER_FIELD_MOD_HEALING_DONE_POS), 235);

    aura->Remove();

    EXPECT_EQ(player->GetInt32Value(PLAYER_FIELD_MOD_DAMAGE_DONE_POS + SPELL_SCHOOL_HOLY), 100);
    EXPECT_EQ(player->GetInt32Value(PLAYER_FIELD_MOD_HEALING_DONE_POS), 100);
}
}
