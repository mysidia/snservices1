/**
 * @file chantrig.h
 * \brief Channel triggers utility headers
 *
 * \mysid
 * \date 2002
 *
 * $Id$ *
 */

void FreeChannelTrigger(ChanTrigger* ct);
ChanTrigger* MakeChannelTrigger(const char* cn);
void AddChannelTrigger(ChanTrigger* ct);
void DelChannelTrigger(ChanTrigger* ct);
ChanTrigger* FindChannelTrigger(const char* name);
unsigned int ChanMaxAkicks(RegChanList* cn);
unsigned int ChanMaxOps(RegChanList* cn);

