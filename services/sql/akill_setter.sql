CREATE SEQUENCE akill_setter_id_seq;

--
-- Record of setter nickname for akill e-mail reports
--
CREATE TABLE akill_setter (
     setter_id   integer DEFAULT nextval('akill_setter_id_seq'),

     nickname  varchar(80),

     -- Number of additions since last report
     current_adds integer,

     -- Number of removals
     current_removes integer,

     -- Number of additions and removals ever performed by
     -- this setter nickname.
     lifetime_adds integer,
     lifetime_removes integer,

     excluded boolean   -- Exclude individual akills from enumeration
			-- for this setter?
);
