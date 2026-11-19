/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU AGPL v3 license, you may redistribute it
 * and/or modify it under version 3 of the License, or (at your option), any later version.
 */

#ifndef _PLAYERBOT_ACCEPTWINTERGRASPINVITATIONACTION_H
#define _PLAYERBOT_ACCEPTWINTERGRASPINVITATIONACTION_H

#include "Action.h"

class PlayerbotAI;

class AcceptWintergraspWarAction : public Action
{
public:
    AcceptWintergraspWarAction(PlayerbotAI* botAI) : Action(botAI, "accept wg war") {}

    bool Execute(Event event) override;
};

class AcceptWintergraspQueueAction : public Action
{
public:
    AcceptWintergraspQueueAction(PlayerbotAI* botAI) : Action(botAI, "accept wg queue") {}

    bool Execute(Event event) override;
};

#endif
