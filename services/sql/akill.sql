CREATE SEQUENCE akill_id_seq;

CREATE TABLE akills (
	akill_id	integer DEFAULT nextval('akill_id_seq'),
	active		boolean NOT NULL DEFAULT 't',
	nick		VARCHAR(80)  NOT NULL default '',
	user		VARCHAR(40)  NOT NULL default '',
	host		VARCHAR(255) NOT NULL default '',
	type		VARCHAR(15),
	added_by	VARCHAR(80) NOT NULL default '',
	duration	integer,
	add_time	ABSTIME NOT NULL,
	expire_time	ABSTIME,
	reason		VARCHAR(255),
	timer_id        INTEGER,

        PRIMARY KEY(akill_id)
);
