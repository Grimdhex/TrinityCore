-- Missing aura for several creatures
-- Magmadar
UPDATE `creature_template_addon` SET `auras` = '19449' WHERE `entry` = 11982;
-- Molten Giant
UPDATE `creature_template_addon` SET `auras` = '18943' WHERE  `entry` = 11658;
UPDATE `creature_addon` SET `auras` = '18943' WHERE `guid` IN (SELECT `guid` FROM `creature` WHERE `id` = 11658);
-- Flameguard
UPDATE `creature_template_addon` SET `auras` = '19626' WHERE  `entry` = 11667;
UPDATE `creature_addon` SET `auras` = '19626' WHERE `guid` IN (SELECT `guid` FROM `creature` WHERE `id` = 11667);
-- Firelord
UPDATE `creature_template_addon` SET `auras` = '19396' WHERE  `entry` = 11668;
UPDATE `creature_addon` SET `auras` = '19396' WHERE `guid` IN (SELECT `guid` FROM `creature` WHERE `id` = 11668);
-- Firesworn
UPDATE `creature_template_addon` SET `auras` = '8876 15733' WHERE `entry` = 12099;
UPDATE `creature_addon` SET `auras` = '8876 15733' WHERE `guid` IN (SELECT `guid` FROM `creature` WHERE `id` = 12099);
-- Lava Reaver
UPDATE `creature_template_addon` SET `auras` = '12787' WHERE `entry` = 12100;
UPDATE `creature_addon` SET `auras` = '12787' WHERE `guid` IN (SELECT `guid` FROM `creature` WHERE `id` = 12100);
-- Ancient Core Hound
UPDATE `creature_template_addon` SET `auras`='18950' WHERE  `entry` = 11658;
UPDATE `creature_addon` SET `auras` = '18950' WHERE `guid` IN (SELECT `guid` FROM `creature` WHERE `id` = 11658);
-- Golemagg
UPDATE `creature_template_addon` SET `auras` = '13879 18943' WHERE  `entry` = 11988;
-- Core Rager
UPDATE `creature_template_addon` SET `auras` = '12787' WHERE  `entry` = 11672;
UPDATE `creature_addon` SET `auras` = '12787' WHERE `guid` IN (SELECT `guid` FROM `creature` WHERE `id` =  11672);
