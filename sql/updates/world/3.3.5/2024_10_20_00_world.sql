-- Core Hound formations
-- GROUP 1
-- 56638, 56635, 56637, 56634, 56636
set @leader := 56638;
DELETE FROM `creature_formations` WHERE `memberGUID` IN (@leader, 56635, 56637, 56634, 56636);
DELETE FROM `creature_formations` WHERE `leaderGUID` IN (@leader, 56635, 56637, 56634, 56636);
INSERT INTO `creature_formations` (`leaderGUID`, `memberGUID`, `dist`, `angle`, `groupAI`, `point_1`, `point_2`) VALUES
(@leader, @leader, 0, 0, 2, 0, 0),
(@leader, 56635, 0, 0, 3, 0, 0),
(@leader, 56637, 0, 0, 3, 0, 0),
(@leader, 56634, 0, 0, 3, 0, 0),
(@leader, 56636, 0, 0, 3, 0, 0);
-- GROUP 2
-- 56629, 56630, 56632, 56633, 56631
set @leader := 56629;
DELETE FROM `creature_formations` WHERE `memberGUID` IN (@leader, 56630, 56632, 56633, 56631);
DELETE FROM `creature_formations` WHERE `leaderGUID` IN (@leader, 56630, 56632, 56633, 56631);
INSERT INTO `creature_formations` (`leaderGUID`, `memberGUID`, `dist`, `angle`, `groupAI`, `point_1`, `point_2`) VALUES
(@leader, @leader, 0, 0, 2, 0, 0),
(@leader, 56630, 0, 0, 3, 0, 0),
(@leader, 56632, 0, 0, 3, 0, 0),
(@leader, 56633, 0, 0, 3, 0, 0),
(@leader, 56631, 0, 0, 3, 0, 0);
-- GROUP 3
-- 56639, 56641, 56642, 56643, 56640
set @leader := 56639;
DELETE FROM `creature_formations` WHERE `memberGUID` IN (@leader, 56641, 56642, 56643, 56640);
DELETE FROM `creature_formations` WHERE `leaderGUID` IN (@leader, 56641, 56642, 56643, 56640);
INSERT INTO `creature_formations` (`leaderGUID`, `memberGUID`, `dist`, `angle`, `groupAI`, `point_1`, `point_2`) VALUES
(@leader, @leader, 0, 0, 2, 0, 0),
(@leader, 56641, 0, 0, 3, 0, 0),
(@leader, 56642, 0, 0, 3, 0, 0),
(@leader, 56643, 0, 0, 3, 0, 0),
(@leader, 56640, 0, 0, 3, 0, 0);
-- GROUP 4
-- 56651, 56650, 56652, 56653, 56654
set @leader := 56651;
DELETE FROM `creature_formations` WHERE `memberGUID` IN (@leader, 56650, 56652, 56653, 56654);
DELETE FROM `creature_formations` WHERE `leaderGUID` IN (@leader, 56650, 56652, 56653, 56654);
INSERT INTO `creature_formations` (`leaderGUID`, `memberGUID`, `dist`, `angle`, `groupAI`, `point_1`, `point_2`) VALUES
(@leader, @leader, 0, 0, 2, 0, 0),
(@leader, 56650, 0, 0, 3, 0, 0),
(@leader, 56652, 0, 0, 3, 0, 0),
(@leader, 56653, 0, 0, 3, 0, 0),
(@leader, 56654, 0, 0, 3, 0, 0);
-- GROUP 5
-- 56648, 56644, 56646, 56649, 56647
set @leader := 56648;
DELETE FROM `creature_formations` WHERE `memberGUID` IN (@leader, 56644, 56646, 56649, 56647);
DELETE FROM `creature_formations` WHERE `leaderGUID` IN (@leader, 56644, 56646, 56649, 56647);
INSERT INTO `creature_formations` (`leaderGUID`, `memberGUID`, `dist`, `angle`, `groupAI`, `point_1`, `point_2`) VALUES
(@leader, @leader, 0, 0, 2, 0, 0),
(@leader, 56644, 0, 0, 3, 0, 0),
(@leader, 56646, 0, 0, 3, 0, 0),
(@leader, 56649, 0, 0, 3, 0, 0),
(@leader, 56647, 0, 0, 3, 0, 0);
