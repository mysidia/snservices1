CREATE TABLE clonerules (
	id		int4 auto_increment NOT NULL PRIMARY KEY,
	active		BOOL default '1',
	user		varchar(80) NOT NULL,
	host		varchar(255) NOT NULL,
	flags		int4 default '0',
	utrig		int4 default NULL,
	htrig		int4 default NULL,
	added_by	varchar(30),
	added_at	DATETIME,
	INDEX(active)
);
