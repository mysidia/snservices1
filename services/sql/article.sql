CREATE SEQUENCE article_id_seq;

CREATE TABLE articles (
	id		nextval('article_id_seq'),
	poster		varchar(80),
	header		varchar(80),
	body		TEXT,
	sent_at		ABSTIME UNIQUE,
	importance	integer,
	PRIMARY KEY(id)
);
