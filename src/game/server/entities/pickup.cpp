/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <game/generated/protocol.h>
#include <game/server/gamecontext.h>
#include <game/block_manager.h> // MineTee
#include "pickup.h"

CPickup::CPickup(CGameWorld *pGameWorld, int Type, int SubType)
: CEntity(pGameWorld, CGameWorld::ENTTYPE_PICKUP)
{
	m_Type = Type;
	m_Subtype = SubType;
	m_ProximityRadius = PickupPhysSize;

	// MineTee
	m_TimerOwnerTake = Server()->Tick();

	m_Vel = vec2(1.0f, 0.0f);
	m_Amount = 0;

	if (m_Type == POWERUP_BLOCK)
        m_DestroyTick = Server()->Tick()+Server()->TickSpeed()*5;
	else if (m_Type == POWERUP_FOOD || m_Type == POWERUP_DROPITEM)
        m_DestroyTick = Server()->Tick()+Server()->TickSpeed()*25;
	else
        m_DestroyTick = -1;
	//

	Reset();

	GameWorld()->InsertEntity(this);
}

void CPickup::Reset()
{
	if (g_pData->m_aPickups[m_Type].m_Spawndelay > 0)
		m_SpawnTick = Server()->Tick() + Server()->TickSpeed() * g_pData->m_aPickups[m_Type].m_Spawndelay;
	else
		m_SpawnTick = -1;
}

void CPickup::Tick()
{
	// MineTee: wait for destroy
	if (m_DestroyTick > 0)
	{
        if (Server()->Tick() > m_DestroyTick)
        {
            GameWorld()->DestroyEntity(this);
            return;
        }
	}

	// wait for respawn
	if(m_SpawnTick > 0)
	{
		if(Server()->Tick() > m_SpawnTick)
		{
			// respawn
			m_SpawnTick = -1;

			if(m_Type == POWERUP_WEAPON)
				GameServer()->CreateSound(m_Pos, SOUND_WEAPON_SPAWN);
		}
		else
			return;
	}

	// MineTee
	CCharacter *pChr = 0x0;
	if (m_Type == POWERUP_DROPITEM)
	{
        GameServer()->Collision()->MoveBox(&m_Pos, &m_Vel, vec2(32.0f, 32.0f), 0.5f);
        m_Vel.y += GameServer()->m_World.m_Core.m_Tuning.m_Gravity;
        m_Vel.x += (m_Vel.x < 0.0f)?0.05f:-0.05f;

        if (Server()->Tick()-m_TimerOwnerTake > Server()->TickSpeed()*2.5f)
            pChr = GameServer()->m_World.ClosestCharacter(m_Pos, 20.0f, 0);
        else if (GameServer()->m_apPlayers[m_Owner] && GameServer()->m_apPlayers[m_Owner]->GetCharacter() && GameServer()->m_apPlayers[m_Owner]->GetCharacter()->IsAlive())
            pChr = GameServer()->m_World.ClosestCharacter(m_Pos, 20.0f, GameServer()->m_apPlayers[m_Owner]->GetCharacter());
	}
    else
        pChr = GameServer()->m_World.ClosestCharacter(m_Pos, 20.0f, 0);
	//

	// Check if a player intersected us
	if(pChr && pChr->IsAlive())
	{
		// player picked us up, is someone was hooking us, let them go
		int RespawnTime = -1;
		switch (m_Type)
		{
			// MineTee
			case POWERUP_DROPITEM:
				if (m_Subtype >= NUM_WEAPONS-CBlockManager::MAX_BLOCKS)
				{
					if (pChr->GiveBlock(m_Subtype, m_Amount))
					{
						GameServer()->CreateSound(m_Pos, SOUND_PICKUP_BLOCK);
						GameWorld()->DestroyEntity(this);
						return;
					}
				}
				else
				{
					if(pChr->GiveWeapon(m_Subtype, m_Amount))
					{
						RespawnTime = g_pData->m_aPickups[m_Type].m_Respawntime;

						if(m_Subtype == WEAPON_GRENADE)
							GameServer()->CreateSound(m_Pos, SOUND_PICKUP_GRENADE);
						else if(m_Subtype == WEAPON_SHOTGUN)
							GameServer()->CreateSound(m_Pos, SOUND_PICKUP_SHOTGUN);
						else if(m_Subtype == WEAPON_RIFLE)
							GameServer()->CreateSound(m_Pos, SOUND_PICKUP_SHOTGUN);

						if(pChr->GetPlayer())
							GameServer()->SendWeaponPickup(pChr->GetPlayer()->GetCID(), m_Subtype);
					}
				}
				break;
			case POWERUP_BLOCK:
				if (pChr->GiveBlock(m_Subtype, 1))
				{
					GameServer()->CreateSound(m_Pos, SOUND_PICKUP_BLOCK);
					GameWorld()->DestroyEntity(this);
					return;
				}
				break;
			case POWERUP_FOOD:
				if (m_Subtype == FOOD_COW)
				{
					if (pChr->IncreaseHealth(2) || pChr->IncreaseArmor(1))
					{
						GameServer()->CreateSound(m_Pos, SOUND_PICKUP_HEALTH);
						GameServer()->CreateSound(m_Pos, SOUND_PICKUP_ARMOR);
						GameWorld()->DestroyEntity(this);
						return;
					}
				}
				else if (m_Subtype == FOOD_PIG)
				{
					if (pChr->IncreaseHealth(1))
					{
						GameServer()->CreateSound(m_Pos, SOUND_PICKUP_HEALTH);
						GameWorld()->DestroyEntity(this);
						return;
					}
				}
				break;
			//
			case POWERUP_HEALTH:
				if(pChr->IncreaseHealth(1))
				{
					GameServer()->CreateSound(m_Pos, SOUND_PICKUP_HEALTH);
					RespawnTime = g_pData->m_aPickups[m_Type].m_Respawntime;
				}
				break;

			case POWERUP_ARMOR:
				if(pChr->IncreaseArmor(1))
				{
					GameServer()->CreateSound(m_Pos, SOUND_PICKUP_ARMOR);
					RespawnTime = g_pData->m_aPickups[m_Type].m_Respawntime;
				}
				break;

			case POWERUP_WEAPON:
				if(m_Subtype >= 0 && m_Subtype < NUM_WEAPONS)
				{
					if(pChr->GiveWeapon(m_Subtype, 10))
					{
						RespawnTime = g_pData->m_aPickups[m_Type].m_Respawntime;

						if(m_Subtype == WEAPON_GRENADE)
							GameServer()->CreateSound(m_Pos, SOUND_PICKUP_GRENADE);
						else if(m_Subtype == WEAPON_SHOTGUN)
							GameServer()->CreateSound(m_Pos, SOUND_PICKUP_SHOTGUN);
						else if(m_Subtype == WEAPON_RIFLE)
							GameServer()->CreateSound(m_Pos, SOUND_PICKUP_SHOTGUN);

						if(pChr->GetPlayer())
							GameServer()->SendWeaponPickup(pChr->GetPlayer()->GetCID(), m_Subtype);
					}
				}
				break;

			case POWERUP_NINJA:
				{
					// activate ninja on target player
					pChr->GiveNinja();
					RespawnTime = g_pData->m_aPickups[m_Type].m_Respawntime;

					// loop through all players, setting their emotes
					CCharacter *pC = static_cast<CCharacter *>(GameServer()->m_World.FindFirst(CGameWorld::ENTTYPE_CHARACTER));
					for(; pC; pC = (CCharacter *)pC->TypeNext())
					{
						if (pC != pChr)
							pC->SetEmote(EMOTE_SURPRISE, Server()->Tick() + Server()->TickSpeed());
					}

					pChr->SetEmote(EMOTE_ANGRY, Server()->Tick() + 1200 * Server()->TickSpeed() / 1000);
					break;
				}

			default:
				break;
		};

		if(RespawnTime >= 0)
		{
			char aBuf[256];
			str_format(aBuf, sizeof(aBuf), "pickup player='%d:%s' item=%d/%d",
				pChr->GetPlayer()->GetCID(), Server()->ClientName(pChr->GetPlayer()->GetCID()), m_Type, m_Subtype);
			GameServer()->Console()->Print(IConsole::OUTPUT_LEVEL_DEBUG, "game", aBuf);
			m_SpawnTick = Server()->Tick() + Server()->TickSpeed() * RespawnTime;
		}
	}
}

void CPickup::TickPaused()
{
	if(m_SpawnTick != -1)
		++m_SpawnTick;
}

void CPickup::Snap(int SnappingClient)
{
	if(m_SpawnTick != -1 || NetworkClipped(SnappingClient))
		return;

	CNetObj_Pickup *pP = static_cast<CNetObj_Pickup *>(Server()->SnapNewItem(NETOBJTYPE_PICKUP, m_ID, sizeof(CNetObj_Pickup)));
	if(!pP)
		return;

	pP->m_X = (int)m_Pos.x;
	pP->m_Y = (int)m_Pos.y;

	// MineTee
	if (m_Type == POWERUP_DROPITEM)
	{
	    pP->m_Type = m_Type + m_Subtype;
        pP->m_Subtype = m_Amount;
	}
	else
	{
        pP->m_Type = m_Type;
        pP->m_Subtype = m_Subtype;
	}
	//
}
