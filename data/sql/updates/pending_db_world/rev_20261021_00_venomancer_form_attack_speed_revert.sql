-- Venomancer Spider Form (52) and Beetle Form (53) fall back to the client DBC rows (CombatRoundTime 0), so both
-- swing at 2.0 s with the equipped weapon's own damage. Removes rows an earlier update gave them; a no-op on fresh databases.
DELETE FROM `spellshapeshiftform_dbc` WHERE `ID` IN (52, 53);
