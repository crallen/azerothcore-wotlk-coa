/* Copyright (C) 2016+ AzerothCore, GNU AGPL v3. */

// coa-stock: CoA's out-of-combat upkeep buffs never expire.
//
// Every CoA class buff cast on the caster or allies that lasts 30 minutes to
// 3 hours (the group buffs in all their ranks, shouts, wards, venoms, weapon
// crafts, tithes) takes SpellDuration.dbc row 21 (-1, "until cancelled")
// once spells load, after CoA's own spell changes, since this script is
// registered last in AddCoAScripts(). The list below is generated from that
// rule by coa-stock's client/gen/permanent_buff_list.py. The Tinker gun
// augmentations need no change here: AscensionTinkerAugmentations.cpp applies
// them with a duration of 0, which never expires. Ally links with
// SPELL_ATTR5_LIMIT_N (Cheers!, Blood Bond, Abyssal Covenant) still end when
// either side leaves the map or logs out.
//
// The startup line this prints is one of coa-stock's check/self-reports: the
// count of auras made permanent, and whether every script bound to them in
// spell_script_names survived. A script whose Validate() requires the old
// duration is unbound silently (the venoms' did, before they accepted -1), so
// a lost binding names the spell and script.
//
// The two marker blocks below are the only list of these IDs: coa-stock's
// client/gen/permanent_buffs.py reads them to rewrite the client's tooltips.

#include "DBCStores.h"
#include "DatabaseEnv.h"
#include "Log.h"
#include "ObjectMgr.h"
#include "ScriptMgr.h"
#include "SpellInfo.h"

#include <algorithm>
#include <array>
#include <string>
#include <vector>

namespace
{
// coa-stock:permanent-auras begin
constexpr std::array<uint32, 451> PermanentAuras = {
    300255, // Starcaller: Celestial Mind
    300855, // Venomancer: Envenomed Weapons
    300856, // Guardian: Honor
    300857, // Barbarian: Brutal Shout
    300858, // Sun Cleric: Devotion of Grace
    300859, // Sun Cleric: Devotion of Grace
    300862, // Sun Cleric: Devotion of Grace
    300863, // Sun Cleric: Devotion of Grace
    300864, // Sun Cleric: Devotion of Grace
    300865, // Sun Cleric: Devotion of Grace
    300866, // Sun Cleric: Devotion of Grace
    300882, // Barbarian: Brutal Shout
    300883, // Barbarian: Brutal Shout
    300884, // Barbarian: Brutal Shout
    300885, // Barbarian: Brutal Shout
    300886, // Barbarian: Brutal Shout
    300887, // Barbarian: Brutal Shout
    300916, // Templar: Gift of Zeal
    300917, // Templar: Gift of Zeal
    300918, // Templar: Gift of Zeal
    300919, // Templar: Gift of Zeal
    300923, // Templar: Gift of Zeal
    300924, // Templar: Gift of Zeal
    301222, // Starcaller: Celestial Mind
    301223, // Starcaller: Celestial Mind
    301224, // Starcaller: Celestial Mind
    301225, // Starcaller: Celestial Mind
    301228, // Guardian: Honor
    301229, // Guardian: Honor
    301230, // Guardian: Honor
    301231, // Guardian: Honor
    301232, // Guardian: Honor
    500751, // Cultist: Abyssal Covenant
    500967, // Necromancer: Razorice
    501326, // Felsworn: Illidari Intuition
    501327, // Felsworn: Illidari Intuition
    501328, // Felsworn: Illidari Intuition
    501329, // Felsworn: Illidari Intuition
    501332, // Felsworn: Illidari Intuition
    501333, // Felsworn: Illidari Intuition
    501334, // Felsworn: Illidari Intuition
    501335, // Felsworn: Illidari Intuition
    501336, // Felsworn: Illidari Intuition
    501486, // Starcaller: Grace of the Moon
    501662, // Bloodmage: Bloodthorns
    501663, // Bloodmage: Bloodthorns
    501664, // Bloodmage: Bloodthorns
    501665, // Bloodmage: Bloodthorns
    501666, // Bloodmage: Bloodthorns
    501667, // Bloodmage: Bloodthorns
    501668, // Bloodmage: Bloodthorns
    501669, // Bloodmage: Bloodthorns
    501670, // Bloodmage: Bloodthorns
    501960, // Necromancer: Razorice
    501961, // Necromancer: Razorice
    501962, // Necromancer: Razorice
    501963, // Necromancer: Razorice
    501964, // Necromancer: Razorice
    501965, // Necromancer: Razorice
    501966, // Necromancer: Razorice
    501967, // Necromancer: Razorice
    501968, // Necromancer: Razorice
    503319, // Stormbringer: Call of the Wind
    503320, // Stormbringer: Call of the Wind
    503321, // Stormbringer: Call of the Wind
    503322, // Stormbringer: Call of the Wind
    503323, // Stormbringer: Call of the Wind
    504296, // Bloodmage: Blood Shield
    504627, // Bloodmage: Blood Bond
    504707, // Pyromancer: Ember Skin
    504720, // Pyromancer: Ashen Skin
    504849, // Bloodmage: Vital Shield
    504897, // Bloodmage: Coagulated Shield
    523478, // Felsworn: Man'ari Intuition
    523479, // Felsworn: Man'ari Intuition
    523480, // Felsworn: Man'ari Intuition
    523481, // Felsworn: Man'ari Intuition
    523482, // Felsworn: Man'ari Intuition
    523483, // Felsworn: Man'ari Intuition
    523484, // Felsworn: Man'ari Intuition
    523485, // Witch Hunter: Knight's Edict
    523486, // Witch Hunter: Knight's Edict
    523487, // Witch Hunter: Knight's Edict
    523488, // Witch Hunter: Knight's Edict
    523489, // Ranger: Footpad's Adaptation
    523490, // Ranger: Footpad's Adaptation
    523491, // Ranger: Footpad's Adaptation
    523492, // Ranger: Footpad's Adaptation
    523493, // Ranger: Footpad's Adaptation
    523494, // Ranger: Footpad's Adaptation
    523495, // Felsworn: Greater Man'ari Intuition
    523510, // Witch Hunter: Greater Knight's Edict
    523513, // Ranger: Greater Footpad's Adaptation
    536258, // Chronomancer: Temporal Resilience
    536259, // Chronomancer: Temporal Resilience
    536260, // Chronomancer: Temporal Resilience
    536261, // Chronomancer: Temporal Resilience
    536262, // Chronomancer: Temporal Resilience
    536263, // Chronomancer: Temporal Resilience
    560289, // Cultist: Void Blessing
    560290, // Cultist: Void Blessing
    560291, // Cultist: Void Blessing
    560292, // Cultist: Void Blessing
    560293, // Cultist: Void Blessing
    560294, // Witch Doctor: Spirit Wuju
    560295, // Runemaster: Etching of the Magi
    560653, // Templar: Harmonic Disposition
    561140, // Witch Doctor: Spirit Wuju
    561141, // Witch Doctor: Spirit Wuju
    561142, // Witch Doctor: Spirit Wuju
    561143, // Witch Doctor: Spirit Wuju
    561236, // Runemaster: Etching of the Leylines
    561237, // Runemaster: Etching of the Dextrous
    561238, // Runemaster: Etching of the Dextrous
    561239, // Runemaster: Etching of the Dextrous
    561240, // Runemaster: Etching of the Dextrous
    561241, // Runemaster: Greater Etching of the Dextrous
    561242, // Runemaster: Greater Etching of the Leylines
    561243, // Runemaster: Greater Etching of the Magi
    561244, // Runemaster: Inscription: Frost
    561245, // Runemaster: Inscription: Frost
    561246, // Runemaster: Inscription: Frost
    561247, // Runemaster: Greater Inscription: Frost
    561248, // Runemaster: Inscription: Fire
    561249, // Runemaster: Inscription: Fire
    561250, // Runemaster: Inscription: Fire
    561251, // Runemaster: Greater Inscription: Fire
    561252, // Runemaster: Inscription: Arcane
    561253, // Runemaster: Inscription: Arcane
    561254, // Runemaster: Inscription: Arcane
    561255, // Runemaster: Greater Inscription: Arcane
    561256, // Runemaster: Inscription: Nature
    561257, // Runemaster: Inscription: Nature
    561258, // Runemaster: Inscription: Nature
    561259, // Runemaster: Greater Inscription: Nature
    561260, // Runemaster: Inscription: Leyline
    561261, // Runemaster: Inscription: Leyline
    561262, // Runemaster: Inscription: Leyline
    561263, // Runemaster: Inscription: Leyline
    561264, // Runemaster: Greater Inscription: Leyline
    561386, // Cultist: Whispers of N'Zoth
    561387, // Cultist: Greater Whispers of N'zoth
    561389, // Cultist: Whispers of Y'shaarj
    561390, // Cultist: Whispers of Y'shaarj
    561391, // Cultist: Whispers of Y'shaarj
    561392, // Cultist: Greater Whispers of Y'shaarj
    570124, // Starcaller: Grace of the Moon
    570170, // Pyromancer: Greater Seal of Alysrazor
    570752, // Primalist: Earthen Endurance
    570753, // Primalist: Earthen Endurance
    570754, // Primalist: Earthen Endurance
    570755, // Primalist: Earthen Endurance
    570756, // Primalist: Greater Earthen Endurance
    572116, // Bloodmage: Greater Bloodthorns
    572173, // Necromancer: Chill of the Tomb
    572174, // Necromancer: Chill of the Tomb
    572175, // Necromancer: Chill of the Tomb
    572176, // Necromancer: Chill of the Tomb
    572177, // Necromancer: Greater Chill of the Tomb
    572214, // Necromancer: Greater Razorice
    572384, // Sun Cleric: Devotion of Dawn
    572385, // Sun Cleric: Devotion of Dawn
    572386, // Sun Cleric: Devotion of Dawn
    572387, // Sun Cleric: Devotion of Dawn
    572388, // Sun Cleric: Devotion of Dawn
    572389, // Sun Cleric: Devotion of Dawn
    572390, // Sun Cleric: Greater Devotion of Dawn
    572391, // Chronomancer: Nozdormu's Wisdom
    572392, // Chronomancer: Nozdormu's Wisdom
    572393, // Chronomancer: Nozdormu's Wisdom
    572394, // Chronomancer: Nozdormu's Wisdom
    572395, // Chronomancer: Nozdormu's Wisdom
    572396, // Chronomancer: Greater Nozdormu's Wisdom
    572400, // Bloodmage: Bloodsoaked Offering
    572401, // Bloodmage: Bloodsoaked Offering
    572402, // Bloodmage: Bloodsoaked Offering
    572403, // Bloodmage: Bloodsoaked Offering
    572404, // Bloodmage: Greater Bloodsoaked Offering
    572408, // Necromancer: Mass Grave
    572552, // Sun Cleric: Devotion of Emperors
    572553, // Sun Cleric: Greater Devotion of Emperors
    572629, // Templar: Gift of Fervor
    572630, // Templar: Greater Gift of Fervor
    572787, // Necromancer: Grim Mandate
    572788, // Necromancer: Grim Mandate
    572789, // Necromancer: Grim Mandate
    572790, // Necromancer: Greater Grim Mandate
    572791, // Cultist: Whispers of C'thun
    572808, // Templar: Righteous Custodian
    572810, // Primalist: Grove Instinct
    572811, // Primalist: Grove Instinct
    572812, // Primalist: Grove Instinct
    572813, // Primalist: Grove Instinct
    572814, // Primalist: Grove Instinct
    572815, // Primalist: Grove Instinct
    572816, // Primalist: Grove Instinct
    572817, // Primalist: Greater Grove Instinct
    572819, // Cultist: Whispers of C'thun
    572905, // Cultist: Whispers of C'thun
    573067, // Cultist: Greater Whispers of C'thun
    573295, // Necromancer: Foul Mandate
    573296, // Necromancer: Foul Mandate
    573297, // Necromancer: Foul Mandate
    573298, // Necromancer: Foul Mandate
    573343, // Starcaller: Arcane Protection
    573344, // Starcaller: Arcane Protection
    573345, // Starcaller: Arcane Protection
    573346, // Starcaller: Arcane Protection
    573347, // Starcaller: Arcane Protection
    573348, // Starcaller: Greater Arcane Protection
    573349, // Primalist: Primal Instinct
    575040, // Sun Cleric: Devotion of Radiance
    575041, // Sun Cleric: Devotion of Radiance
    575042, // Sun Cleric: Devotion of Radiance
    575043, // Sun Cleric: Devotion of Radiance
    575044, // Sun Cleric: Devotion of Radiance
    575045, // Sun Cleric: Greater Devotion of Radiance
    575839, // Reaper: Rite of Perseverance
    575840, // Reaper: Rite of Perseverance
    575841, // Reaper: Rite of Perseverance
    575842, // Reaper: Greater Rite of Perseverance
    575843, // Stormbringer: Call of the Lightning
    575844, // Stormbringer: Call of the Lightning
    575845, // Stormbringer: Call of the Lightning
    575846, // Stormbringer: Greater Call of the Lightning
    578126, // Reaper: Rite of Power
    578127, // Reaper: Rite of Power
    578128, // Reaper: Rite of Power
    578129, // Reaper: Rite of Power
    578130, // Reaper: Greater Rite of Power
    578311, // Stormbringer: Call of the Storm
    578312, // Stormbringer: Call of the Storm
    578313, // Stormbringer: Call of the Storm
    578314, // Stormbringer: Call of the Storm
    578315, // Stormbringer: Call of the Storm
    578316, // Stormbringer: Greater Call of the Storm
    578344, // Witch Doctor: Resourceful Wuju
    582533, // Guardian: Fire Protection
    582534, // Guardian: Fire Protection
    582535, // Guardian: Fire Protection
    582536, // Guardian: Greater Fire Protection
    628324, // Chronomancer: Source of Order
    628327, // Chronomancer: Source of Chaos
    630868, // Venomancer: Rejuvenating Venom
    630871, // Venomancer: Fang Venom: Toxic Rejuvenation
    630880, // Venomancer: Fang Venom: Toxic Blast
    630882, // Venomancer: Fang Venom: Lifeblood
    630883, // Venomancer: Fang Venom: Blight
    630884, // Venomancer: Fang Venom: Weakening
    674202, // Necromancer: Hardened Flesh
    680280, // Guardian: Greater Honor
    680282, // Stormbringer: Whirlwind Aegis
    680286, // Necromancer: Greater Foul Mandate
    680289, // Cultist: Greater Void Blessing
    680291, // Stormbringer: Greater Call of the Wind
    680294, // Ranger: Greater Woodsman's Adaptation
    680298, // Reaper: Greater Rite of Resolve
    680299, // Bloodmage: Greater Sanguinary Offering
    680300, // Knight of Xoroth: Greater Mark of Korth’azz
    680301, // Starcaller: Greater Celestial Mind
    680302, // Barbarian: Enduring Shout
    680303, // Witch Hunter: Greater Inquisitor's Edict
    680306, // Templar: Greater Gift of Zeal
    680307, // Chronomancer: Greater Chromie's Wisdom
    680308, // Felsworn: Greater Illidari Intuition
    680310, // Primalist: Greater Primal Instinct
    680312, // Venomancer: Greater Spider Pheromones
    680315, // Tinker: Greater Power Module
    680316, // Stormbringer: Shocking Aegis
    680334, // Stormbringer: Tempest Aegis
    680387, // Pyromancer: Magma Skin
    680388, // Necromancer: Fetid Ward
    680389, // Chronomancer: Temporal Resilience
    680390, // Chronomancer: Temporal Swiftness
    680456, // Chronomancer: Temporal Restoration
    680590, // Stormbringer: Whirlwind Aegis
    680591, // Stormbringer: Whirlwind Aegis
    680592, // Stormbringer: Whirlwind Aegis
    680593, // Stormbringer: Whirlwind Aegis
    680594, // Stormbringer: Whirlwind Aegis
    680872, // Witch Doctor: Greater Spirit Wuju
    681160, // Sun Cleric: Greater Devotion of Grace
    681314, // Pyromancer: Dragon Skin
    681439, // Barbarian: Enduring Shout
    681440, // Barbarian: Enduring Shout
    681441, // Barbarian: Enduring Shout
    681442, // Witch Hunter: Greater Witching Edict
    681460, // Necromancer: Glacial Ward
    681527, // Ranger: Wild Blessing
    681529, // Necromancer: Bone Ward
    681538, // Ranger: Wild Blessing
    681539, // Ranger: Wild Blessing
    681793, // Necromancer: Bone Ward
    706589, // Knight of Xoroth: Mark of Korth'azz
    706630, // Bloodmage: Sanguinary Offering
    706634, // Templar: Gift of Zeal
    706741, // Witch Hunter: Inquisitor's Edict
    706742, // Tinker: Power Module
    707336, // Bloodmage: Sanguinary Offering
    707337, // Bloodmage: Sanguinary Offering
    707338, // Bloodmage: Sanguinary Offering
    707339, // Bloodmage: Sanguinary Offering
    707340, // Bloodmage: Sanguinary Offering
    707341, // Knight of Xoroth: Mark of Korth'azz
    707342, // Knight of Xoroth: Mark of Korth'azz
    707343, // Knight of Xoroth: Mark of Korth'azz
    707344, // Knight of Xoroth: Mark of Korth'azz
    707345, // Knight of Xoroth: Mark of Korth'azz
    707346, // Tinker: Power Module
    707347, // Tinker: Power Module
    707348, // Tinker: Power Module
    707349, // Tinker: Power Module
    707350, // Tinker: Power Module
    707351, // Witch Hunter: Inquisitor's Edict
    707352, // Witch Hunter: Inquisitor's Edict
    707353, // Witch Hunter: Inquisitor's Edict
    707354, // Witch Hunter: Inquisitor's Edict
    707355, // Witch Hunter: Inquisitor's Edict
    707671, // Witch Doctor: Power Wuju
    707672, // Witch Doctor: Power Wuju
    707673, // Witch Doctor: Power Wuju
    707674, // Witch Doctor: Power Wuju
    707675, // Witch Doctor: Power Wuju
    707676, // Witch Doctor: Power Wuju
    707677, // Witch Doctor: Power Wuju
    707678, // Witch Hunter: Inquisitor's Edict
    707679, // Witch Hunter: Inquisitor's Edict
    707680, // Witch Hunter: Inquisitor's Edict
    707681, // Witch Hunter: Inquisitor's Edict
    707682, // Witch Hunter: Inquisitor's Edict
    707683, // Witch Hunter: Inquisitor's Edict
    707684, // Witch Hunter: Witching Edict
    707685, // Witch Hunter: Witching Edict
    707686, // Witch Hunter: Witching Edict
    707687, // Witch Hunter: Witching Edict
    707688, // Tinker: Power Module
    707689, // Venomancer: Toxic Pheromones
    707690, // Venomancer: Toxic Pheromones
    707691, // Venomancer: Toxic Pheromones
    707692, // Venomancer: Toxic Pheromones
    707693, // Knight of Xoroth: Mark of Blaumeux
    707694, // Knight of Xoroth: Mark of Blaumeux
    707695, // Knight of Xoroth: Mark of Blaumeux
    707696, // Knight of Xoroth: Mark of Blaumeux
    712458, // Witch Doctor: Greater Power Wuju
    712459, // Venomancer: Greater Toxic Pheromones
    712460, // Knight of Xoroth: Greater Mark of Blaumeux
    800194, // Cultist: Void Blessing
    800195, // Witch Doctor: Greater Resourceful Wuju
    800196, // Pyromancer: Seal of Alysrazor
    800197, // Primalist: Primal Instinct
    800198, // Reaper: Rite of Resolve
    800199, // Necromancer: Foul Mandate
    800212, // Felsworn: Illidari Intuition
    800266, // Ranger: Woodsman's Adaptation
    800270, // Ranger: Rider's Harness
    800272, // Ranger: Read Map
    800852, // Sun Cleric: Devotion of Grace
    801517, // Tinker: Tinkering Tools
    801523, // Chronomancer: Chromie's Wisdom
    802313, // Bloodmage: Bloodthorns
    802601, // Ranger: Twineweave Line
    802809, // Ranger: Skinner's Grip
    802819, // Pyromancer: Seal of Alysrazor
    802820, // Pyromancer: Seal of Alysrazor
    802821, // Pyromancer: Seal of Alysrazor
    802822, // Pyromancer: Seal of Alysrazor
    802827, // Chronomancer: Chromie's Wisdom
    802828, // Chronomancer: Chromie's Wisdom
    802829, // Chronomancer: Chromie's Wisdom
    802830, // Chronomancer: Chromie's Wisdom
    802831, // Chronomancer: Chromie's Wisdom
    802832, // Chronomancer: Chromie's Wisdom
    802833, // Chronomancer: Chromie's Wisdom
    802834, // Chronomancer: Chromie's Wisdom
    803177, // Venomancer: Spider Pheromones
    803306, // Venomancer: Spider Pheromones
    803307, // Venomancer: Spider Pheromones
    803308, // Venomancer: Spider Pheromones
    803309, // Venomancer: Spider Pheromones
    803310, // Reaper: Rite of Resolve
    803311, // Reaper: Rite of Resolve
    803312, // Reaper: Rite of Resolve
    803313, // Reaper: Rite of Resolve
    803314, // Reaper: Rite of Resolve
    803315, // Primalist: Primal Instinct
    803316, // Primalist: Primal Instinct
    803317, // Primalist: Primal Instinct
    803318, // Primalist: Primal Instinct
    803319, // Primalist: Primal Instinct
    803320, // Ranger: Woodsman's Adaptation
    803321, // Ranger: Woodsman's Adaptation
    803322, // Ranger: Woodsman's Adaptation
    803323, // Ranger: Woodsman's Adaptation
    803324, // Ranger: Woodsman's Adaptation
    803460, // Pyromancer: Flint Blessing
    803649, // Pyromancer: Seal of Al'ar
    803650, // Venomancer: Spider Pheromones
    803651, // Venomancer: Beetle Pheromones
    803652, // Venomancer: Beetle Pheromones
    803653, // Venomancer: Beetle Pheromones
    803654, // Venomancer: Beetle Pheromones
    803655, // Venomancer: Beetle Pheromones
    803656, // Venomancer: Beetle Pheromones
    803657, // Venomancer: Greater Beetle Pheromones
    803658, // Tinker: Mana Module
    803659, // Tinker: Mana Module
    803660, // Tinker: Mana Module
    803661, // Tinker: Mana Module
    803662, // Tinker: Mana Module
    803663, // Tinker: Mana Module
    803664, // Tinker: Mana Module
    803665, // Tinker: Greater Mana Module
    803666, // Ranger: Woodsman's Adaptation
    803667, // Knight of Xoroth: Mark of Rivendare
    803668, // Knight of Xoroth: Mark of Rivendare
    803669, // Knight of Xoroth: Mark of Rivendare
    803670, // Knight of Xoroth: Mark of Rivendare
    803671, // Knight of Xoroth: Mark of Zeliek
    803729, // Pyromancer: Seal of Al'ar
    803730, // Reaper: Greater Mark of Rivendare
    803731, // Knight of Xoroth: Greater Mark of Zeliek
    804018, // Stormbringer: Call of the Wind
    804541, // Pyromancer: Dragonscales
    804667, // Witch Hunter: Charming Conversation
    804782, // Templar: Tithe of Crusading
    804783, // Templar: Tithe of Order
    804784, // Templar: Tithe of Might
    804815, // Templar: Tithe of Liberty
    804816, // Templar: Tithe of Heroism
    804817, // Templar: Tithe of Elements
    804818, // Templar: Tithe of Alacrity
    804819, // Templar: Tithe of Mysticism
    804820, // Templar: Tithe of Courage
    804821, // Templar: Tithe of Ingenuity
    805597, // Knight of Xoroth: Greater Demon's Might
    805731, // Venomancer: Debilitating Venom
    805775, // Venomancer: Adrenal Venom
    805776, // Venomancer: Blight Venom
    805777, // Venomancer: Nullifying Venom
    805778, // Venomancer: Weakening Venom
    805810, // Barbarian: Cheers!
    807704, // Pyromancer: Seal of Al'ar
    807770, // Pyromancer: Seal of Al'ar
    807798, // Venomancer: Venom Shield
    808012, // Pyromancer: Seal of Al'ar
    808060, // Pyromancer: Greater Seal of Al'ar
    808941, // Ranger: Weapon Craft: Jagged Edge
    808961, // Ranger: Weapon Craft: Oil Dip
    808967, // Ranger: Weapon Craft: Nerve Venom
    809823, // Ranger: Weapon Craft: Freezing Toxin
};
// coa-stock:permanent-auras end
static_assert(std::is_sorted(PermanentAuras.begin(), PermanentAuras.end()), "the lookup is a binary search");

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

uint32 madePermanent = 0;

class coa_stock_permanent_buffs : public GlobalScript
{
public:
    coa_stock_permanent_buffs() : GlobalScript("coa_stock_permanent_buffs",
        {GLOBALHOOK_ON_LOAD_SPELL_CUSTOM_ATTR}) { }

    void OnLoadSpellCustomAttr(SpellInfo* info) override
    {
        if (!std::binary_search(PermanentAuras.begin(), PermanentAuras.end(), info->Id))
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
        // Every binding spell_script_names asks for, against what survived Validate().
        std::string ids;
        for (uint32 spellId : PermanentAuras)
            ids += (ids.empty() ? "" : ",") + std::to_string(spellId);

        uint32 expected = 0;
        std::vector<std::string> lost;
        if (QueryResult result = WorldDatabase.Query("SELECT spell_id, ScriptName FROM spell_script_names WHERE spell_id IN ({})", ids))
        {
            do
            {
                Field* fields = result->Fetch();
                uint32 const spellId = fields[0].Get<uint32>();
                std::string const name = fields[1].Get<std::string>();
                ++expected;
                bool bound = false;
                SpellScriptsBounds bounds = sObjectMgr->GetSpellScriptsBounds(spellId);
                for (auto itr = bounds.first; itr != bounds.second && !bound; ++itr)
                    bound = sObjectMgr->GetScriptName(itr->second) == name;
                if (!bound)
                    lost.push_back(std::to_string(spellId) + " " + name);
            } while (result->NextRow());
        }

        if (lost.empty())
            LOG_INFO("coa", "CoAStock permanent-buffs: {}/{} auras permanent, every script binding kept",
                madePermanent, PermanentAuras.size());
        else
        {
            std::string names;
            for (std::string const& binding : lost)
                names += (names.empty() ? "" : ", ") + binding;
            LOG_INFO("coa", "CoAStock permanent-buffs: {}/{} auras permanent, {} of {} script bindings lost: {}",
                madePermanent, PermanentAuras.size(), lost.size(), expected, names);
        }
    }
    }
};
}

void AddSC_CoAStockPermanentBuffs()
{
    new coa_stock_permanent_buffs();
    new coa_stock_permanent_buffs_report();
}
