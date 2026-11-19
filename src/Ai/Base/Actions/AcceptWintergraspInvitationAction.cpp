/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU AGPL v3 license, you may redistribute it
 * and/or modify it under version 3 of the License, or (at your option), any later version.
 */

#include "AcceptWintergraspInvitationAction.h"

#include "Battlefield.h"
#include "Event.h"
#include "Opcodes.h"
#include "PlayerbotAI.h"
#include "WorldPacket.h"
#include "WorldSession.h"

bool AcceptWintergraspWarAction::Execute(Event /*event*/)
{
    WorldPacket packet(CMSG_BATTLEFIELD_MGR_ENTRY_INVITE_RESPONSE, 5);
    packet << uint32(BATTLEFIELD_BATTLEID_WG);
    packet << uint8(1);
    bot->GetSession()->HandleBfEntryInviteResponse(packet);

    return true;
}

bool AcceptWintergraspQueueAction::Execute(Event /*event*/)
{
    WorldPacket packet(CMSG_BATTLEFIELD_MGR_QUEUE_INVITE_RESPONSE, 5);
    packet << uint32(BATTLEFIELD_BATTLEID_WG);
    packet << uint8(1);
    bot->GetSession()->HandleBfQueueInviteResponse(packet);

    return true;
}
