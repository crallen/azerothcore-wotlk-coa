/* Copyright (C) 2016+ AzerothCore, GNU AGPL v3. */

// wow-realms: advancement, free-pick serves a stock class on the Warcraft Reborn realm, so the
// player's legacy talent opcodes are refused for it. Defined in src/server/coa/RealmsAdvancement.cpp.

#ifndef REALMS_LEGACY_TALENTS_H
#define REALMS_LEGACY_TALENTS_H

class Player;

namespace Realms
{
    bool RefusesLegacyTalents(Player const* player);
}

#endif
