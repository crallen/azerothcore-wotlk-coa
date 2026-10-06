/* Copyright (C) 2016+ AzerothCore, GNU AGPL v3. */

// wow-realms: every script this project carries registers here, so a rebase touches one of our
// files, never the tail of CoAScriptLoader.cpp.

void AddSC_RealmsPoisonTheWorld();
void AddSC_RealmsAdvancement();

void AddRealmsScripts()
{
    AddSC_RealmsPoisonTheWorld();
    AddSC_RealmsAdvancement();
}
