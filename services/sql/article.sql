CREATE TABLE articles (
	id		int4 auto_increment PRIMARY KEY,
	poster		varchar(80),
	header		varchar(80),
	body		TEXT,
	sent_at		DATETIME,
	importance	int4,
	INDEX(sent_at)
);
