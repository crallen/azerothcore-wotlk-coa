/* Copyright (C) 2016+ AzerothCore, GNU AGPL v3. */

// wow-realms: the copy binding's seams in the core, driven through a test player with the pair map
// bound over spells placed in the SpellInfo store.

#include "DBCStores.h"
#include "IntegrationTestFixture.h"
#include "RealmsBinding.h"
#include "SpellInfoTestHelper.h"
#include "SpellMgr.h"

namespace
{
constexpr uint32 SpellStoreSize = 1200000;
constexpr uint32 ShieldWall = 871;
constexpr uint32 ShieldWallCopy = 1100871;
constexpr uint32 SharedCategory = 1209;
constexpr uint32 ShieldWallCooldown = 300000;
constexpr uint32 CategoryCooldown = 12000;

// A namesake in the store whose copy is bound but missing from the store: a cast that resolves
// to the copy fails its store lookup before any cast machinery runs.
constexpr uint32 BattleShout = 6673;
constexpr uint32 UnstoredBattleShoutCopy = 1106673;

class RealmsBindingSeamTest : public IntegrationTestFixture
{
protected:
    void SetUp() override
    {
        IntegrationTestFixture::SetUp();
        for (uint32 id : { ShieldWall, ShieldWallCopy })
        {
            SpellInfo* spell = SpellInfoBuilder()
                                   .WithId(id)
                                   .WithSpellFamilyName(SPELLFAMILY_WARRIOR)
                                   .WithEffect(0, SPELL_EFFECT_APPLY_AURA, SPELL_AURA_DUMMY)
                                   .Build();
            spell->CategoryEntry = &category;
            spell->RecoveryTime = ShieldWallCooldown;
            spell->CategoryRecoveryTime = CategoryCooldown;
            sSpellMgr->AddSpellInfoForTest(id, spell);
        }
        sSpellMgr->AddSpellInfoForTest(BattleShout, SpellInfoBuilder().WithId(BattleShout).Build());
        unstoredCopy = SpellInfoBuilder().WithId(UnstoredBattleShoutCopy).BuildUnique();
        sSpellsByCategoryStore[SharedCategory] = { { false, ShieldWall }, { false, ShieldWallCopy } };

        Realms::Bind({ { ShieldWallCopy, ShieldWall }, { UnstoredBattleShoutCopy, BattleShout } }, {},
            SpellStoreSize,
            [this](uint32 id)
            {
                return id == UnstoredBattleShoutCopy ? unstoredCopy.get() : const_cast<SpellInfo*>(sSpellMgr->GetSpellInfo(id));
            });
    }

    void TearDown() override
    {
        Realms::ClearBinding();
        sSpellsByCategoryStore.erase(SharedCategory);
        sSpellMgr->UnloadSpellInfoStore();
        IntegrationTestFixture::TearDown();
    }

    TestPlayer* CreateWarrior()
    {
        TestPlayer* player = CreateTestPlayer();
        player->SetByteValue(UNIT_FIELD_BYTES_0, 1, CLASS_WARRIOR);
        return player;
    }

    SpellCategoryEntry category { SharedCategory, 0 };
    std::unique_ptr<SpellInfo> unstoredCopy;
};
}

TEST_F(RealmsBindingSeamTest, ACategorisedCopyKeepsItsOwnCooldownWhenItsNamesakeSharesTheCategory)
{
    TestPlayer* warrior = CreateWarrior();

    warrior->AddSpellAndCategoryCooldowns(sSpellMgr->GetSpellInfo(ShieldWallCopy), 0);

    SpellCooldowns const& cooldowns = warrior->GetSpellCooldownMap();
    ASSERT_EQ(cooldowns.count(ShieldWallCopy), 1u);
    SpellCooldown const& cooldown = cooldowns.at(ShieldWallCopy);
    EXPECT_EQ(cooldown.maxduration, ShieldWallCooldown);
    EXPECT_EQ(cooldown.category, 0u);
    EXPECT_TRUE(cooldown.needSendToClient);
    EXPECT_EQ(cooldowns.count(ShieldWall), 0u);
}

TEST_F(RealmsBindingSeamTest, CooldownQueriesByTheNamesakeSeeTheCopysCooldown)
{
    TestPlayer* warrior = CreateWarrior();
    warrior->AddSpellCooldown(ShieldWallCopy, 0, ShieldWallCooldown);

    EXPECT_TRUE(warrior->HasSpellCooldown(ShieldWall));

    warrior->RemoveSpellCooldown(ShieldWall);

    EXPECT_FALSE(warrior->HasSpellCooldown(ShieldWallCopy));
}

TEST_F(RealmsBindingSeamTest, CastingANamesakeByIdLooksUpTheCopyForAStockClassPlayer)
{
    TestPlayer* warrior = CreateWarrior();

    EXPECT_EQ(warrior->CastSpell(warrior, BattleShout, true), SPELL_FAILED_SPELL_UNAVAILABLE);
    EXPECT_EQ(warrior->CastCustomSpell(BattleShout, CustomSpellValues(), warrior), SPELL_FAILED_SPELL_UNAVAILABLE);
}
