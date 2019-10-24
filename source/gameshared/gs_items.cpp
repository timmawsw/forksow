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

// gs_items.c	-	game shared items definitions

#include "q_arch.h"
#include "q_math.h"
#include "q_shared.h"
#include "q_comref.h"
#include "q_collision.h"
#include "gs_public.h"

/*
*
* ITEM DEFS
*
*/

const Item itemdefs[] = {
	{
		Item_Gunblade,

		"Knife", "gb",
		RGB8( 255, 255, 255 ),
		"Knife people in the face",
		0,

		NULL, NULL, NULL
	},

	{
		Item_MachineGun,

		"AK-69", "mg",
		RGB8( 254, 235, 98 ),
		"Shoots fast direct bullets touching enemies at any range",
		100,

		NULL, NULL, NULL
	},

	{
		Item_Shotgun,

		"Shotgun", "rg",
		RGB8( 255, 172, 30 ),
		"Basically a shotgun",
		100,

		NULL, NULL, NULL
	},

	{
		Item_GrenadeLauncher,

		"Grenades", "gl",
		RGB8( 62, 141, 255 ),
		"Deprecated gun, enjoy it while it lasts nerds",
		100,

		PATH_GRENADE_MODEL,
		NULL, NULL
	},

	{
		Item_RocketLauncher,

		"Rockets", "rl",
		RGB8( 255, 58, 66 ),
		"Shoots slow moving rockets that deal damage in an area and push bodies away",
		200,

		PATH_ROCKET_MODEL,
		S_WEAPON_ROCKET_FLY,
		NULL
	},

	{
		Item_Plasma,

		"Plasma", "pg",
		RGB8( 172, 80, 255 ),
		"Shoots fast projectiles that deal damage in an area",
		100,

		PATH_PLASMA_MODEL,
		S_WEAPON_PLASMAGUN_FLY,
		NULL
	},

	{
		Item_Laser,

		"Laser", "lg",
		RGB8( 82, 252, 95 ),
		"Shoots a continuous trail doing quick but low damage at a certain range",
		200,

		NULL,
		S_WEAPON_LASERGUN_HUM " "
			S_WEAPON_LASERGUN_STOP " "
			S_WEAPON_LASERGUN_HIT_0 " " S_WEAPON_LASERGUN_HIT_1 " " S_WEAPON_LASERGUN_HIT_2,
		NULL
	},

	{
		Item_Railgun,

		"Railgun", "eb",
		RGB8( 80, 243, 255 ),
		"Shoots a direct laser hit doing pretty high damage",
		200,

		NULL,
		S_WEAPON_ELECTROBOLT_HIT,
		NULL
	},

	{ Item_WeaponCount },

	{ Item_Bomb },

	{
		Item_FakeBomb,

		"Dummy bomb", "dummybomb",
		RGB8( 255, 100, 255 ),
		"Hello",
		100,

		NULL, NULL, NULL
	},
};

STATIC_ASSERT( ARRAY_COUNT( itemdefs ) == Item_Count );

const Item * GS_FindItemByType( ItemType type ) {
	assert( type >= 0 && type < Item_Count );
	assert( type == itemdefs[ type ].type );
	return &itemdefs[ type ];
}

const Item * GS_FindItemByName( const char * name ) {
	for( size_t i = 0; i < ARRAY_COUNT( itemdefs ); i++ ) {
		if( itemdefs[ i ].name == NULL )
			continue;
		if( Q_stricmp( name, itemdefs[ i ].name ) == 0 )
			return &itemdefs[ i ];
		if( Q_stricmp( name, itemdefs[ i ].shortname ) == 0 )
			return &itemdefs[ i ];
	}

	return NULL;
}

const bool GS_CanEquip( const player_state_t * playerState, ItemType type ) {
	if( !playerState->inventory[ type ] ) {
		return false;
	}

	if( !( playerState->pmove.stats[ PM_STAT_FEATURES ] & PMFEAT_WEAPONSWITCH ) ) {
		return false;
	}

	if( type == playerState->stats[ STAT_PENDING_WEAPON ] ) {
		return false;
	}

	return true;
}
