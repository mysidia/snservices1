/**
 * \file clone.c
 * \brief Services clone detection
 *
 * Procedures found in this module are used for tracking 
 * and alerting the network to the presence of possible clonebots.
 *
 * \wd \taz \greg \mysid
 * \date 1996-1997, 1999, 2001
 *
 * $Id$
 */

/*
 * Copyright (c) 1996-1997 Chip Norkus
 * Copyright (c) 1997 Max Byrd
 * Copyright (c) 1997 Greg Poma
 * Copyright (c) 1999, 2001 James Hess
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the authors nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHORS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "services.h"
#include "nickserv.h"
#include "hash.h"

HostClone *firstClone = NULL;	/*!< List of clone info items */
HostClone *lastClone = NULL;	/*!< Last entry in clone info items */
CloneRule *first_crule = NULL;	/*!< First clone rule item */

/**
 * A pending clone alert item
 *
 * To be valid 'hc' must point to a valid hostclone record
 * and next must be a null pointer or point to a valid pending_alert
 * record.
 */
struct pending_alert
{
	HostClone		*hc;			///< The host-clone record alerted
	char			ha, ua;			///< Just a user alert, or a host alert too?
	int 			signon_ct,		///< Number signons during the alert period
					signoff_ct; 	///< Number signoffs during alert period

	int			signon_ct_r;
	int			signoff_ct_r;

	time_t			firstTime,		///< Timestamp alert started
					lastAlert;  	///< Timestamp last alert issued

	struct pending_alert *next;		///< Next pending alert in list
} *pendingAlerts;		///< List of pending clone alerts


/**
 * \brief Find a pending clone alert
 * \param hc Host object find a pending alert for
 * \pre Hc is a pointer to a valid, listed hostclone object.
 * \return A null pointer is returned if no clone alert could be
 *         found to match the given object.  Else a pointer to
 *         the proper alert object.
 */
struct pending_alert* find_palert(HostClone *hc)
{
	struct pending_alert *pI;

	for(pI = pendingAlerts; pI; pI = pI->next)
		if (pI->hc == hc)
			break;
	return pI;
}

/**
 * \brief Create a new clone alert
 * \return A pointer to a freshly-allocated, initialized clone alert object
 */
struct pending_alert* make_palert()
{
	struct pending_alert *pa = 
		(struct pending_alert *)oalloc(sizeof(pending_alert));

	pa->ua = pa->ha = 0;
	pa->hc = NULL;
	pa->next = NULL;

	return pa;
}

/**
 * \param pArg Clone alert to free out
 * \pre pArg is a pointer to an otherwise valid, clone alert record that
 *      has been removed from the alert list.
 * \post The area pointed to by pArg is freed.
 */
void free_palert(struct pending_alert *pArg)
{
	FREE(pArg);
}

/**
 * \brief Add a clone alert to the list.
 * \param pArg Record to list
 * \pre pArg is a pointer to a new clone alert record.
 * \post The clone alert record referenced by pArg is listed
 *       and ready for use if otherwise valid.
 */
void add_palert(struct pending_alert *pArg)
{
	struct pending_alert *pTmp;

	pTmp = pendingAlerts;
	pendingAlerts = pArg;
	pArg->next = pTmp;
}

/**
 * \brief A clone alert is removed from the list
 * \param pZap Clone alert to remove
 * \pre pZap is a pointer to a valid, listed clone alert record.
 * \post The alert referenced by pZap is otherwise valid but is no
 *       longer listed.
 */
void remove_palert(struct pending_alert *pZap)
{
	struct pending_alert *pTmp = NULL, *pI;

	for(pI = pendingAlerts; pI; pI = pI->next)
	{
		if (pI == pZap) {
			if (pTmp)
				pTmp->next = pZap->next;
			else
				pendingAlerts = pZap->next;
			return;
		}
		else
			pTmp = pI;
	}
}

/**
 * \brief Send the list of clone alerts to a user /OS TRIGGER LIST
 * \pre Nick is a pointer to a valid, listed online user.
 * \post An IRC message that no clone alerts are present or a list of
 *       clone alerts has been sent for nick.
 */
void listCloneAlerts(UserList *nick)
{
	char *from = nick->nick;
	struct pending_alert *pIterator;
	HostClone *hc;
	UserClone *uc;
	float signonRate;
	int found = 0;

	for(pIterator = pendingAlerts; pIterator; pIterator = pIterator->next)
	{
		if (CTime == pIterator->lastAlert)
			continue;

		hc = pIterator->hc;

		if (pIterator->lastAlert)
			signonRate = (pIterator->signon_ct_r - pIterator->signoff_ct_r) /
			             (float)(CTime - pIterator->lastAlert);
		else
			signonRate = 1.0;

		if (hc->clones >= hc->trigger) {
			sSend(":%s NOTICE %s :CLONES(%i) *@%s   (Signon Rate: %.2f)", OperServ, from,
			      hc->clones, hc->host, signonRate);
			found = 1;
			continue;
		}

		for(uc = hc->firstUser; uc; uc = uc->next) {
			if (!(uc->uflags & CLONE_ALERT) || (uc->uflags & CLONE_OK)
			    || (uc->clones < uc->trigger))
				continue;
			sSend(":%s NOTICE %s : -- %i %s@%s     (H: %.2f)", OperServ, from,
			      uc->clones, uc->user, hc->host, signonRate);
			found = 1;
		}
	}

	if (found == 0)
		sSend(":%s NOTICE %s :No clone alerts at the moment.", OperServ, from);
}

/** @brief Update clone alerts
 *  \pre Aval is the UTC calendar time at which the updateCloneAlert procedure
 *      began.
 *  \return The time at which the next regular updateCloneAlert procedure
 *          should occur.
 */
time_t updateCloneAlerts(time_t aval)
{
	struct pending_alert *pIterator;
	float signonRate;
	HostClone *hc;
	UserClone *uc;

	aval += 60;

	for(pIterator = pendingAlerts; pIterator; pIterator = pIterator->next)
	{
		if (CTime == pIterator->lastAlert || (pIterator->lastAlert + 10) >= CTime)
		    continue;

		if (pIterator->lastAlert) {
			if (CTime - pIterator->lastAlert != 0)
				signonRate = (pIterator->signon_ct_r - pIterator->signoff_ct_r) /
				             (float)(CTime - pIterator->lastAlert);
			else
				signonRate = (pIterator->signon_ct_r - pIterator->signoff_ct_r) / 1;
		}
		else
			signonRate = 1.0;


		if (pIterator->signon_ct_r == 0 && (CTime - pIterator->lastAlert) < 1200)
			continue;

		//
		// Don't warn more than once per 60 seconds unless
		// we get a significant increase.
		//
		if (((CTime - pIterator->lastAlert) < 90) &&
		    signonRate < .03)
		    continue;

		if (pIterator->lastAlert && (pIterator->signon_ct - pIterator->signoff_ct) < 0)
			continue;

		pIterator->lastAlert = CTime;
		pIterator->signon_ct_r = 0;
		pIterator->signoff_ct_r = 0;

		hc = pIterator->hc;

		if ((hc->flags & CLONE_ALERT) && !(hc->flags & CLONE_OK)
		    && (hc->clones >= hc->trigger))
			sSend(":%s GLOBOPS :CLONES(%i/%i): *@%s  (%.2f cps)", OperServ, hc->clones, hc->trigger, hc->host, signonRate);

		for(uc = hc->firstUser; uc; uc = uc->next) {
			if (!(uc->uflags & CLONE_ALERT) || (uc->uflags & CLONE_OK)
			    || (uc->clones < uc->trigger))
				continue;
			sSend(":%s GLOBOPS :CLONES(%i/%i): %s@%s", OperServ, uc->clones,
			      uc->trigger, uc->user,
			      hc->host);
		}
	}

	return aval;
}

/************************************************************************/

/** \brief Retrieve a clone rule by mask or number for oper edit purposes
 *  \param mask Name of mask or index number to search for
 *  \return A clone rule object or a NULL pointer indicating that the
 *          sought item could not be found.
 */
CloneRule *
GetCrule(char *mask)
{
	int ct = 0, num = 0;
	CloneRule *crule;

	if (!mask)
		return NULL;

	for (crule = first_crule; crule; crule = crule->next)
		if (crule->mask[0] && !strcasecmp(mask, crule->mask))
			return crule;
	if (isdigit(*mask)) {
		num = atoi(mask);
		for (ct = 1, crule = first_crule; crule; crule = crule->next, ct++)
			if (num == ct)
				return crule;
	}
	return NULL;
}

/** \brief Find any matching rules
 *  \param host Address of user
 *  \return A matching clone rule object or a null pointer
 *          for no match conditions
 */
CloneRule *
GetCruleMatch(char *host)
{
	CloneRule *crule;

	if (!host)
		return NULL;

	for (crule = first_crule; crule; crule = crule->next) {
		if (crule->mask && !match(crule->mask, host))
			return crule;
	}
	return NULL;
}

/** \brief Creates a new clone rule object
 *  \return A pointer to a newly created clone rule item, any
 *          failure is fatal.
 */
CloneRule *
NewCrule()
{
	return (CloneRule *) oalloc(sizeof(CloneRule));
}

/** \brief Add an alocated rule rule to the linked list at point 'n'
 *  \param rule Item to add to rule linklist
 *  \param n Point to insert item into list at
 */
void
AddCrule(CloneRule * rule, int n)
{
	CloneRule *tmp;
	int ct = 0;

	if (!rule)
		return;

	for (tmp = first_crule; tmp; tmp = tmp->next)
		if (rule == tmp)
			return;
	if ((n != -2 && n < 2) || !first_crule) {
		/* insert at the beginning */
		rule->next = first_crule;
		first_crule = rule;
	} else {
		/* insert before point n */
		for (ct = 1, tmp = first_crule; tmp && tmp->next;
			 tmp = tmp->next, ct++)
			if ((((ct + 1) >= n) && n != -2) || !tmp->next)
				break;
		if (tmp) {
			rule->next = tmp->next;
			tmp->next = rule;
		}
	}
}


/**
 *   \brief Build a user@host mask for the purpose of crule checking
 *   \return Returns a pattern to a user@host mask, returns a blank 
 *           string "" on error.  Never returns NULL.
 */
static char *
MakeUserHost(char *user, char *host)
{
	static char maskbuf[HOSTLEN + USERLEN + 10] = "";
	if (snprintf
		(maskbuf, sizeof(maskbuf), "%s@%s", user ? user : "*",
		 host ? host : "*") == 0)
		maskbuf[0] = '\0';
	return maskbuf;
}

/**
 * Apply a crule to a trigger if they are in sync
 *
 * 'orig' is what the rule looked like before the update
 * and is used for the 'sync' determination
 *
 * A rule that is in sync with a trigger either completely matches
 * the trigger or if the trigger completely matches the defaults.
 */
void
UpdateCrule(CloneRule orig, CloneRule * rule)
{
	HostClone *hc;
	UserClone *uc;

	if (!rule)
		return;

	for (hc = firstClone; hc; hc = hc->next) {
		if (
			(hc->trigger != orig.trigger
			 && hc->trigger != DEFHOSTCLONETRIGGER)
			|| (hc->flags != DEFCLONEFLAGS && hc->flags != orig.flags))
			continue;

		if (!match(rule->mask, MakeUserHost("*", hc->host))) {
			if (rule->trigger > 1)
				hc->trigger = rule->trigger;
			else
				hc->trigger = DEFHOSTCLONETRIGGER;
			hc->flags = rule->flags;
		}
		for (uc = hc->firstUser; uc; uc = uc->next) {
			if (uc->trigger != orig.utrigger
				&& uc->trigger != DEFUSERCLONETRIGGER) continue;
			if (!match(rule->mask, MakeUserHost(uc->user, hc->host))) {
				/* Make a rule value of '0' mean "default" */
				if (rule->trigger > 1)
					hc->trigger = rule->trigger;
				else
					hc->trigger = DEFHOSTCLONETRIGGER;
				if (rule->utrigger > 1)
					uc->trigger = rule->utrigger;
				else
					uc->trigger = DEFUSERCLONETRIGGER;
				hc->flags = rule->flags;
			}
		}
	}
}

/** \brief Remove a clone rule from the linked list
 *  \param zap Rule to remove
 */
void
RemoveCrule(CloneRule * zap)
{
	CloneRule *tmp, *rule;
	for (rule = first_crule, tmp = NULL; rule; rule = rule->next) {
		if (rule == zap) {
			if (tmp)
				tmp->next = rule->next;
			else
				first_crule = rule->next;
			return;
		} else
			tmp = rule;
	}
}

/** \brief Initializes a clone host detection structure
 *  \param hc Pointer to clone host object to initialize
 */
void
initCloneData(HostClone * hc)
{
	hc->host[0] = 0;
	hc->firstUser = hc->lastUser = NULL;
	hc->trigger = DEFHOSTCLONETRIGGER;
	hc->clones = 0;
	hc->flags = DEFCLONEFLAGS;
	hc->next = hc->previous = hc->hashnext = hc->hashprev = NULL;
}

/** \brief Registers an online user with the clone detection system
 *  \param nick Nickname of client signing on
 *  \param user Username of client signing on
 *  \param host Hostname of client signing on
 *  \return Normally returns 0, returns 1 if clones are killed
 *          and should not be registered with the online users
 *          database.
 *  \warning If 1 is returned then no online user structure
 *            has been affected, only a KILL message has been
 *            dispatched, and the user has not been registered
 *            with the clone database but instead been rejected.
 */
int
addClone(char *nick, char *user, char *host)
{
	HostClone *hc;
	struct pending_alert *pA;
	HashKeyVal hashEnt;
	UserClone *uc;
	CloneRule *rule;
	int user_alert = 0, host_alert = 0;

	hc = getCloneData(host);
	if (!hc) {
		hc = (HostClone *) oalloc(sizeof(HostClone));
		initCloneData(hc);
		if ((rule = GetCruleMatch(MakeUserHost(user, host)))) {
			if (rule->trigger > 1)
				hc->trigger = rule->trigger;
			hc->flags = rule->flags;
		}
		strcpy(hc->host, host);
		hc->clones = 1;
		if (!firstClone) {
			firstClone = hc;
			hc->previous = NULL;
		} else {
			lastClone->next = hc;
			hc->previous = lastClone;
		}
		lastClone = hc;
		hc->next = NULL;

		hashEnt = getHashKey(host) % CLONEHASHSIZE;
		if (!CloneHash[hashEnt].clone) {
			CloneHash[hashEnt].clone = hc;
			hc->hashprev = NULL;
		} else {
			CloneHash[hashEnt].lastclone->hashnext = hc;
			hc->hashprev = CloneHash[hashEnt].lastclone;
		}
		CloneHash[hashEnt].lastclone = hc;
		hc->hashnext = NULL;
	} else
		hc->clones++;

	uc = addUserClone(hc, user);
	if (hc->flags & CLONE_IGNOREFLAG)
		return 0;

	if (hc->clones >= hc->trigger || uc->clones >= uc->trigger) {
		if (hc->flags & CLONE_KILLFLAG) {
			if (!(rule = GetCruleMatch(MakeUserHost(user, host)))
				|| !rule->kill_msg)
				sSend(":%s KILL %s :%s!%s (Cloning is not allowed on "
					  NETWORK ".)", NickServ, nick, services[1].host,
					  NickServ);
			else
				sSend(":%s KILL %s :%s!%s (%s)", NickServ, nick,
					  services[1].host, NickServ, rule->kill_msg);
			delClone(user, host);
			return 1;
		} else {
			if ((rule = GetCruleMatch(MakeUserHost(user, host)))
				&& rule->warn_msg)
				if (uc->clones == uc->trigger + 1
					|| hc->clones == hc->trigger + 1)
					sSend(":%s NOTICE #%s :%s", OperServ, host,
						  rule->warn_msg);
		}
	}
	if (uc->clones >= uc->trigger && !(uc->uflags & CLONE_ALERT)) {
		uc->uflags &= ~(CLONE_OK);
		uc->uflags |= CLONE_ALERT;
		user_alert = 1;
	}

	if (hc->clones >= hc->trigger
		&& !(hc->flags & CLONE_ALERT))
	{
		hc->flags &= ~(CLONE_OK);
		hc->flags |= CLONE_ALERT;
		host_alert = 1;
	}

	pA = find_palert(hc);

	if (pA) {
		pA->signon_ct++;
		pA->signon_ct_r++;
	}

	if (user_alert || host_alert) {
		if (!pA) {
			pA = make_palert();
			add_palert(pA);
			pA->hc = hc;
			pA->firstTime = time(NULL);
			pA->lastAlert = 0;
			pA->signon_ct = pA->signoff_ct = 0;
			pA->signon_ct_r = pA->signoff_ct_r = 0;
		}

		if (user_alert)
			pA->ua = 1;
		if (host_alert)
			pA->ha = 1;
	}

	(void)updateCloneAlerts(0);
/*
	if (pA)
	{
		if (user_alert)
			sSend(":%s GLOBOPS :CLONES(%i): %s@%s", OperServ, uc->clones, user,
			      host);
		else if (host_alert)
			sSend(":%s GLOBOPS :CLONES(%i): *@%s", OperServ, hc->clones, host);
	}

*/
	return 0;
}

/** \brief Adds a user's username information to the user
 *         clone database for their hostname
 *  \param hc Host-wide clone information
 *  \param user Userid of the person being added
 *  \return The user clone object created or found is returned.
 */
UserClone *
addUserClone(HostClone * hc, char *user)
{
	UserClone *uc = getUserCloneData(hc, user);
	CloneRule *rule;

	if (!uc) {
		uc = (UserClone *) oalloc(sizeof(UserClone));
		uc->user[0] = 0;
		uc->clones = 1;
		uc->trigger = DEFUSERCLONETRIGGER;
		if ((rule = GetCruleMatch(MakeUserHost(user, hc->host)))) {
			if (rule->utrigger > 0)
				uc->trigger = rule->utrigger;
			/** \bug There may be a problem in the logic handling
			 * of user@ rules verses @host rules
			 * maybe mask should be changed to simply 'host'
			 * instead of [<user@>]host
			 */
			if (hc->trigger == DEFHOSTCLONETRIGGER
				&& hc->flags == DEFCLONEFLAGS) {
				if (rule->trigger > 1)
					hc->trigger = rule->trigger;
				hc->flags = rule->flags;
			}
		}
		strcpy(uc->user, user);

		if (!hc->firstUser) {
			hc->firstUser = uc;
			uc->previous = NULL;
		} else {
			hc->lastUser->next = uc;
			uc->previous = hc->lastUser;
		}
		hc->lastUser = uc;
		uc->next = NULL;
	} else
		uc->clones++;

	if (uc->clones >= uc->trigger)
		return uc;
	return uc;
}

/** \brief Remove a client from the clone database
 * \param user Username of client logging off
 * \param host Hostname of client logging off
 */
void
delClone(char *user, char *host)
{
	HostClone *hc = getCloneData(host);
	if (hc) {
		UserClone *uc = getUserCloneData(hc, user);
		if (uc) {
			hc->clones--;
			uc->clones--;

			if (!uc->clones)
				delUserClone(hc, uc);
			if (hc->clones) {
				struct pending_alert* pA = find_palert(hc);

				if (pA) {
					pA->signoff_ct++;
					pA->signoff_ct_r++;
					if ((hc->clones < hc->trigger &&
					    uc->clones < uc->trigger) &&
					    (CTime - pA->firstTime) > 30)
					{
						remove_palert(pA);
						free_palert(pA);
						uc->uflags &= ~CLONE_ALERT;
					}
				}
			}

			if (!hc->clones) {
				HashKeyVal hashEnt;
				HostClone *tmp;
				struct pending_alert* pA = find_palert(hc);

				if (pA) {
					remove_palert(pA);
					free_palert(pA);
					hc->flags &= ~CLONE_ALERT;
				}

				if (hc->previous)
					hc->previous->next = hc->next;
				else
					firstClone = hc->next;
				if (hc->next)
					hc->next->previous = hc->previous;
				else
					lastClone = hc->previous;
				hashEnt = getHashKey(hc->host) % CLONEHASHSIZE;
				for (tmp = CloneHash[hashEnt].clone; tmp;
					 tmp = tmp->hashnext) {
					if (tmp == hc) {
						if (hc->hashprev)
							hc->hashprev->hashnext = hc->hashnext;
						else
							CloneHash[hashEnt].clone = hc->hashnext;
						if (hc->hashnext)
							hc->hashnext->hashprev = hc->hashprev;
						else
							CloneHash[hashEnt].lastclone = hc->hashprev;
					}
				}
				FREE(hc);
			}
		}
	}
}

/**
 * @brief Delete information on a user@ from a cloneinfo machine item
 */
void
delUserClone(HostClone * hc, UserClone * uc)
{
	if (uc->previous)
		uc->previous->next = uc->next;
	else
		hc->firstUser = uc->next;
	if (uc->next)
		uc->next->previous = uc->previous;
	else
		hc->lastUser = uc->previous;

	FREE(uc);
}

/** \brief Retrieves the clone information for a machine
 *  \param host Hostname to retrieve information on
 */
HostClone *
getCloneData(char *host)
{
	HashKeyVal hashEnt;
	hashEnt = getHashKey(host) % CLONEHASHSIZE;
	if (!CloneHash[hashEnt].clone)
		return NULL;
	else {
		HostClone *tmp;
		for (tmp = CloneHash[hashEnt].clone; tmp; tmp = tmp->hashnext) {
			if (!strcasecmp(host, tmp->host))
				return tmp;
		}
		return NULL;
	}
}

/**
 * @brief Get clone-detection-related information on a user from a host clone
 *        record and knowledge of a username.
 */
UserClone *
getUserCloneData(HostClone * hc, char *user)
{
	UserClone *tmp;

	for (tmp = hc->firstUser; tmp; tmp = tmp->next) {
		if (!strcasecmp(tmp->user, user))
			return tmp;
	}
	return NULL;
}

/************************************************************************/
