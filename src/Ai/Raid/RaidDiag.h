/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

// Raid encounter diagnostics (boss-test traces). Off unless AiPlayerbot.RaidDiagnostics = 1, so release builds never
// log them, whatever the playerbots logger level. Read once at first use.

#ifndef PLAYERBOTS_RAIDDIAG_H
#define PLAYERBOTS_RAIDDIAG_H

#include "Config.h"
#include "Log.h"

inline bool RaidDiagEnabled()
{
    static bool const enabled = sConfigMgr->GetOption<bool>("AiPlayerbot.RaidDiagnostics", false, false);
    return enabled;
}

#define RAID_DIAG(filterType__, ...)                 \
    do                                               \
    {                                                \
        if (RaidDiagEnabled())                       \
            LOG_INFO(filterType__, __VA_ARGS__);     \
    } while (0)

#endif
