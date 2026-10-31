SET @ENTRY:=35410;
DELETE FROM `acore_string` WHERE `entry`=@ENTRY;
INSERT INTO `acore_string` (`entry`, `content_default`) VALUES
(@ENTRY, 'Hello World from Skeleton-Module!');

DELETE FROM `acore_string_locale` WHERE `entry`=@ENTRY AND `locale` IN ('esES', 'esMX');
INSERT INTO `acore_string_locale` (`entry`, `locale`, `content`) VALUES
(@ENTRY, 'esES', '¡Hola Mundo desde Skeleton-Module!'),
(@ENTRY, 'esMX', '¡Hola Mundo desde Skeleton-Module!');
