-- Venomancer Spider Form (52) and Beetle Form (53) swing at their own speed, as Cat Form (1.0 s) and
-- Bear Form (2.5 s) do; the client DBC ships both with CombatRoundTime 0. Other columns copy the DBC rows.
DELETE FROM `spellshapeshiftform_dbc` WHERE `ID` IN (52, 53);
INSERT INTO `spellshapeshiftform_dbc`
(`ID`, `BonusActionBar`, `Name_Lang_enUS`, `Name_Lang_Mask`, `Flags`, `CreatureType`, `AttackIconID`,
`CombatRoundTime`, `CreatureDisplayID_1`) VALUES
(52, 1, 'Stalker Form', 16712190, 728, 1, 1256, 1000, 194404),
(53, 3, 'Scorpid Form', 16712190, 728, 1, 17060, 2500, 110063);
