CREATE TABLE akills (
	id		int4 NOT NULL auto_increment PRIMARY KEY,
	active		BOOL NOT NULL default '1',
	nick		VARCHAR(80)  NOT NULL default '',
	user		VARCHAR(40) NOT NULL default '',
	host		VARCHAR(255) NOT NULL default '',
	type		ak_type	ENUM(
				'autokill', 'autohurt', 'ignore', 'zline'
			),

	added_by	VARCHAR(80) NOT NULL default '',
	duration	int4,
	add_time	DATETIME NOT NULL,
	expire_time	DATETIME,
	reason		VARCHAR(255),

	INDEX(active, type)
);
