/*
Copyright (C) 1997-2001 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

// gs_weapons.c	-	game shared weapons definitions

#include "q_arch.h"
#include "q_math.h"
#include "q_shared.h"
#include "q_comref.h"
#include "q_collision.h"
#include "gs_public.h"

/*
* GS_TraceBullet
*/
trace_t *GS_TraceBullet( const gs_state_t * gs, trace_t *trace, vec3_t start, vec3_t dir, vec3_t right, vec3_t up, float r, float u, int range, int ignore, int timeDelta ) {
	vec3_t end;
	bool water = false;
	vec3_t water_start;
	int content_mask = MASK_SHOT | MASK_WATER;
	static trace_t water_trace;

	assert( trace );

	VectorMA( start, range, dir, end );
	if( r ) {
		VectorMA( end, r, right, end );
	}
	if( u ) {
		VectorMA( end, u, up, end );
	}

	if( gs->api.PointContents( start, timeDelta ) & MASK_WATER ) {
		water = true;
		VectorCopy( start, water_start );
		content_mask &= ~MASK_WATER;
	}

	gs->api.Trace( trace, start, vec3_origin, vec3_origin, end, ignore, content_mask, timeDelta );

	// see if we hit water
	if( trace->contents & MASK_WATER ) {
		water_trace = *trace;

		VectorCopy( trace->endpos, water_start );

		// re-trace ignoring water this time
		gs->api.Trace( trace, water_start, vec3_origin, vec3_origin, end, ignore, MASK_SHOT, timeDelta );

		return &water_trace;
	}

	if( water ) {
		water_trace = *trace;
		VectorCopy( water_start, water_trace.endpos );
		return &water_trace;
	}

	return NULL;
}

void GS_TraceLaserBeam( const gs_state_t * gs, trace_t *trace, vec3_t origin, vec3_t angles, float range, int ignore, int timeDelta, void ( *impact )( trace_t *tr, vec3_t dir ) ) {
	vec3_t dir, end;
	vec3_t mins = { -0.5, -0.5, -0.5 };
	vec3_t maxs = { 0.5, 0.5, 0.5 };

	AngleVectors( angles, dir, NULL, NULL );
	VectorMA( origin, range, dir, end );

	trace->ent = 0;

	gs->api.Trace( trace, origin, mins, maxs, end, ignore, MASK_SHOT, timeDelta );
	if( trace->ent != -1 && impact != NULL ) {
		impact( trace, dir );
	}
}


//============================================================
//
//		PLAYER WEAPON THINKING
//
//============================================================

#define NOAMMOCLICK_PENALTY 100

WeaponType GS_SelectBestWeapon( const SyncPlayerState * player ) {
	for( int weapon = Weapon_Count - 1; weapon >= 0; weapon-- ) {
		if( !player->weapons[ weapon ].owned )
			continue;
		return WeaponType( weapon );
	}
	return Weapon_Count;
}

static bool GS_CheckAmmoInWeapon( const SyncPlayerState * player, WeaponType weapon ) {
	const WeaponDef * def = GS_GetWeaponDef( weapon );
	return def->clip_size == 0 || player->weapons[ weapon ].ammo > 0;
}

/*
* GS_ThinkPlayerWeapon
*/
int GS_ThinkPlayerWeapon( const gs_state_t * gs, SyncPlayerState *playerState, int buttons, int msecs, int timeDelta ) {
	bool refire = false;

	assert( playerState->stats[STAT_PENDING_WEAPON] >= 0 && playerState->stats[STAT_PENDING_WEAPON] < Weapon_Count );

	if( GS_MatchPaused( gs ) ) {
		return playerState->stats[STAT_WEAPON];
	}

	if( playerState->pmove.pm_type != PM_NORMAL ) {
		playerState->weaponState = WEAPON_STATE_READY;
		playerState->stats[STAT_PENDING_WEAPON] = Weapon_Count;
		playerState->stats[STAT_WEAPON] = Weapon_Count;
		playerState->stats[STAT_WEAPON_TIME] = 0;
		return playerState->stats[STAT_WEAPON];
	}

	if( playerState->pmove.stats[PM_STAT_NOUSERCONTROL] > 0 ) {
		buttons = 0;
	}

	if( !msecs ) {
		goto done;
	}

	playerState->stats[STAT_WEAPON_TIME] = Max2( 0, playerState->stats[ STAT_WEAPON_TIME ] - msecs );

	const WeaponDef * def = GS_GetWeaponDef( playerState->stats[STAT_WEAPON] );

	// during cool-down time it can shoot again or go into reload time
	if( playerState->weaponState == WEAPON_STATE_REFIRE ) {
		if( playerState->stats[STAT_WEAPON_TIME] > 0 ) {
			goto done;
		}

		refire = true;

		playerState->weaponState = WEAPON_STATE_READY;
	}

	if( playerState->weaponState == WEAPON_STATE_NOAMMOCLICK ) {
		if( playerState->stats[STAT_WEAPON_TIME] > 0 ) {
			goto done;
		}

		if( playerState->stats[STAT_WEAPON] != playerState->stats[STAT_PENDING_WEAPON] ) {
			playerState->weaponState = WEAPON_STATE_READY;
		}
	}

	// there is a weapon to be changed
	if( playerState->stats[STAT_WEAPON] != playerState->stats[STAT_PENDING_WEAPON] ) {
		if( playerState->weaponState == WEAPON_STATE_READY || playerState->weaponState == WEAPON_STATE_ACTIVATING ) {
			playerState->weaponState = WEAPON_STATE_DROPPING;
			playerState->stats[STAT_WEAPON_TIME] += def->weapondown_time;

			if( def->weapondown_time ) {
				gs->api.PredictedEvent( playerState->POVnum, EV_WEAPONDROP, 0 );
			}
		}
	}

	// do the change
	if( playerState->weaponState == WEAPON_STATE_DROPPING ) {
		if( playerState->stats[STAT_WEAPON_TIME] > 0 ) {
			goto done;
		}

		bool had_weapon_before = playerState->stats[STAT_WEAPON] != Weapon_Count;
		playerState->stats[STAT_WEAPON] = playerState->stats[STAT_PENDING_WEAPON];

		// update the firedef
		def = GS_GetWeaponDef( playerState->stats[STAT_WEAPON] );
		playerState->weaponState = WEAPON_STATE_ACTIVATING;
		playerState->stats[STAT_WEAPON_TIME] += def->weaponup_time;

		int parm = playerState->stats[STAT_WEAPON] << 1;
		if( !had_weapon_before )
			parm |= 1;

		gs->api.PredictedEvent( playerState->POVnum, EV_WEAPONACTIVATE, parm );
	}

	if( playerState->weaponState == WEAPON_STATE_ACTIVATING ) {
		if( playerState->stats[STAT_WEAPON_TIME] > 0 ) {
			goto done;
		}

		playerState->weaponState = WEAPON_STATE_READY;
	}

	if( playerState->weaponState == WEAPON_STATE_READY || playerState->weaponState == WEAPON_STATE_NOAMMOCLICK ) {
		if( playerState->stats[STAT_WEAPON_TIME] > 0 ) {
			goto done;
		}

		if( !GS_ShootingDisabled( gs ) ) {
			if( buttons & BUTTON_ATTACK ) {
				if( GS_CheckAmmoInWeapon( playerState, playerState->stats[STAT_WEAPON] ) ) {
					playerState->weaponState = WEAPON_STATE_FIRING;
				}
				else if( playerState->weaponState != WEAPON_STATE_NOAMMOCLICK ) {
					// player has no ammo nor clips
					playerState->weaponState = WEAPON_STATE_NOAMMOCLICK;
					playerState->stats[STAT_WEAPON_TIME] += NOAMMOCLICK_PENALTY;
					gs->api.PredictedEvent( playerState->POVnum, EV_NOAMMOCLICK, 0 );
					goto done;
				}
			}
		}
	}

	if( playerState->weaponState == WEAPON_STATE_FIRING ) {
		int parm = playerState->stats[STAT_WEAPON] << 1;

		playerState->stats[STAT_WEAPON_TIME] += def->reload_time;
		playerState->weaponState = WEAPON_STATE_REFIRE;

		if( refire && def->smooth_refire ) {
			gs->api.PredictedEvent( playerState->POVnum, EV_SMOOTHREFIREWEAPON, parm );
		} else {
			gs->api.PredictedEvent( playerState->POVnum, EV_FIREWEAPON, parm );
		}

		if( def->clip_size > 0 ) {
			playerState->weapons[ playerState->stats[ STAT_WEAPON ] ].ammo--;
			if( playerState->weapons[ playerState->stats[ STAT_WEAPON ] ].ammo == 0 ) {
				gs->api.PredictedEvent( playerState->POVnum, EV_NOAMMOCLICK, 0 );
			}
		}
	}
done:
	return playerState->stats[STAT_WEAPON];
}
