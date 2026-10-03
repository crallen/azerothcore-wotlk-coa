-- coa-stock: Poison the World (Mysterious Concoction) credits the selected NPC.
DELETE FROM `spell_script_names` WHERE `spell_id` = 685013 AND `ScriptName` = 'spell_coa_stock_poison_the_world';
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(685013, 'spell_coa_stock_poison_the_world');
