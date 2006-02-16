CREATE SEQUENCE clonerule_id_seq;

CREATE TABLE clonerules (
	id		integer nextval('clonerule_id_seq'),
	active		boolean default 't',
	user		varchar(80) NOT NULL,
	host		varchar(255) NOT NULL,
	flags		integer default '0',
	utrig		integer default NULL,
	htrig		integer default NULL,
	added_by	varchar(30),
	added_at	abstime,
	PRIMARY KEY(id)
);
