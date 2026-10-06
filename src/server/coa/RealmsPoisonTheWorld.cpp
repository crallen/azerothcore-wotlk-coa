/* Copyright (C) 2016+ AzerothCore, GNU AGPL v3. */

// wow-realms: the Mysterious Concoction (item 662217) of the Venomancer quests
// Poisoning the World (200024-200026) splashes the NPC the player has
// selected.
//
// The item casts Poison the World (685013), a dummy on TARGET_UNIT_TARGET_ANY
// that each quest NPC's SmartAI answers on spell hit with its kill credit. The
// CoA client sends the item use with the player as the target even with the
// NPC selected (a packet capture shows CMSG_USE_ITEM's unit target is the
// caster), so the spell hits the player and no NPC ever sees it. When the
// spell lands on its own caster, this hands the hit to the selected creature
// within the spell's range, so the SmartAI credit fires as authored.
//
// The startup line is one of wow-realms' check/self-reports.

#include "Creature.h"
#include "CreatureAI.h"
#include "Log.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "SpellInfo.h"
#include "SpellScript.h"

namespace
{
constexpr uint32 SPELL_POISON_THE_WORLD = 685013;
}

class spell_realms_poison_the_world : public SpellScript
{
    PrepareSpellScript(spell_realms_poison_the_world);

    void HandleDummy(SpellEffIndex /*effIndex*/)
    {
        Player* player = GetCaster() ? GetCaster()->ToPlayer() : nullptr;
        if (!player || GetHitUnit() != player)
            return;

        Creature* selected = ObjectAccessor::GetCreature(*player, player->GetTarget());
        if (!selected || !selected->IsAIEnabled || !selected->IsAlive())
            return;

        if (!player->IsWithinDistInMap(selected, GetSpellInfo()->GetMaxRange(true, player)))
            return;

        selected->AI()->SpellHit(player, GetSpellInfo());
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_realms_poison_the_world::HandleDummy, EFFECT_0, SPELL_EFFECT_DUMMY);
    }
};

void AddSC_RealmsPoisonTheWorld()
{
    RegisterSpellScript(spell_realms_poison_the_world);
    LOG_INFO("coa", "Realms poison-the-world: active");
}
