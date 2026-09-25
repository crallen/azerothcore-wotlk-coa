/* Copyright (C) 2016+ AzerothCore, GNU AGPL v3. */

// coa-stock: CoA's out-of-combat upkeep buffs never expire.
//
// The seven 30-minute maintenance buffs and the six 2-hour Venomancer venoms
// take SpellDuration.dbc row 21 (-1, "until cancelled") once spells load,
// after CoA's own spell changes, since this script is registered last in
// AddCoAScripts(). The six Tinker gun augmentations need no change here:
// AscensionTinkerAugmentations.cpp applies them with a duration of 0, which
// never expires. The ally links (Cheers!, Blood Bond, Abyssal Covenant) are
// SPELL_ATTR5_LIMIT_N, which the core still drops when either side leaves
// the map or logs out.
//
// The startup line this prints is one of coa-stock's check/self-reports: the
// count of auras made permanent, and how many venoms still have all three
// venom scripts bound (their Validate requires the 2-hour duration or -1, so
// a lost edit there unbinds them silently).
//
// The two marker blocks below are the only list of these IDs: coa-stock's
// client/gen/permanent_buffs.py reads them to rewrite the client's tooltips.

#include "DBCStores.h"
#include "Log.h"
#include "ObjectMgr.h"
#include "ScriptMgr.h"
#include "SpellInfo.h"

#include <algorithm>
#include <array>
#include <string>

namespace
{
// coa-stock:permanent-auras begin
constexpr std::array<uint32, 13> PermanentAuras = {
    805810, // Cheers!
    504627, // Blood Bond
    500751, // Abyssal Covenant
    300855, // Envenomed Weapons
    680388, // Fetid Ward
    681460, // Glacial Ward
    681529, // Bone Ward
    630868, // Rejuvenating Venom
    805731, // Debilitating Venom
    805775, // Adrenal Venom
    805776, // Blight Venom
    805777, // Nullifying Venom
    805778, // Weakening Venom
};
// coa-stock:permanent-auras end

// Not used by the server (see the header): read by coa-stock's generator.
// coa-stock:permanent-augmentations begin
constexpr std::array<uint32, 6> PermanentAugmentations = {
    653130, // Aether Augmentation
    653234, // Piercing Augmentation
    653236, // Explosive Augmentation
    653239, // Stim Augmentation
    653242, // Magic Augmentation
    653245, // Tracer Augmentation
};
// coa-stock:permanent-augmentations end
static_assert(PermanentAugmentations.size() == 6, "the six Tinker gun augmentations");

constexpr uint32 DurationUntilCancelled = 21;

constexpr std::array<uint32, 6> Venoms = { 630868, 805731, 805775, 805776, 805777, 805778 };
constexpr std::array<char const*, 3> VenomScripts = {
    "spell_ascension_venomancer_venom_selection",
    "aura_ascension_venomancer_venom_selection",
    "aura_ascension_venomancer_venom_proc",
};

uint32 madePermanent = 0;

class coa_stock_permanent_buffs : public GlobalScript
{
public:
    coa_stock_permanent_buffs() : GlobalScript("coa_stock_permanent_buffs",
        {GLOBALHOOK_ON_LOAD_SPELL_CUSTOM_ATTR}) { }

    void OnLoadSpellCustomAttr(SpellInfo* info) override
    {
        if (std::find(PermanentAuras.begin(), PermanentAuras.end(), info->Id) == PermanentAuras.end())
            return;

        SpellDurationEntry const* untilCancelled = sSpellDurationStore.LookupEntry(DurationUntilCancelled);
        if (!untilCancelled)
        {
            LOG_ERROR("coa", "CoAStock permanent-buffs: SpellDuration.dbc has no row {}; spell {} keeps its duration",
                DurationUntilCancelled, info->Id);
            return;
        }

        info->DurationEntry = untilCancelled;
        ++madePermanent;
    }
};

class coa_stock_permanent_buffs_report : public WorldScript
{
public:
    coa_stock_permanent_buffs_report() : WorldScript("coa_stock_permanent_buffs_report", {WORLDHOOK_ON_STARTUP}) { }

    void OnStartup() override
    {
        uint32 scripted = 0;
        for (uint32 venom : Venoms)
        {
            SpellScriptsBounds bounds = sObjectMgr->GetSpellScriptsBounds(venom);
            uint32 bound = 0;
            for (char const* name : VenomScripts)
                for (auto itr = bounds.first; itr != bounds.second; ++itr)
                    if (sObjectMgr->GetScriptName(itr->second) == name)
                    {
                        ++bound;
                        break;
                    }
            if (bound == VenomScripts.size())
                ++scripted;
        }

        LOG_INFO("coa", "CoAStock permanent-buffs: {}/{} auras permanent, {}/{} venoms scripted",
            madePermanent, PermanentAuras.size(), scripted, Venoms.size());
    }
};
}

void AddSC_CoAStockPermanentBuffs()
{
    new coa_stock_permanent_buffs();
    new coa_stock_permanent_buffs_report();
}
