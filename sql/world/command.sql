DELETE FROM `command` WHERE name IN ('qcl', 'qcl view', 'qcl enable', 'qcl disable');

INSERT INTO `command` (`name`, `security`, `help`) VALUES 
('qcl', 0, 'Syntax: .qcl $subcommand\nType .help qcl to see a list of subcommands\nor .help qcl $subcommand to see info on the subcommand.'),
('qcl view', 0, 'Syntax: .qcl view\nView your current quest count.'),
('qcl enable', 0, 'Syntax: .qcl enable\nEnable quest count leveling.'),
('qcl disable', 0, 'Syntax: .qcl disable\nDisable quest count leveling.');
