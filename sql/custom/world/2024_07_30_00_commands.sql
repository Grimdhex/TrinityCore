-- NPC GUID command
DELETE FROM `command` WHERE `name` = "npc guid";
INSERT INTO `command` (`name`, `help`) VALUES 
("npc guid", "Syntax: .npc guid");
