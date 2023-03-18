CREATE TABLE IF NOT EXISTS `questcountlevel` (
  `CharacterGUID` int(11) NOT NULL,
  `QuestCount` int(11) NOT NULL DEFAULT '0',
  `Enabled` boolean DEFAULT false,
  PRIMARY KEY (`CharacterGUID`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;
