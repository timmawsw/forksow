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
#include "game/g_local.h"

void G_UseItem( edict_t *ent, const Item *it ) {
	if( it != NULL && it->type & IT_WEAPON ) {
		Use_Weapon( ent, it );
	}
}

static void PrecacheAssets( const char * s, int precache( const char * ) ) {
	char data[MAX_QPATH];

	while( *s ) {
		const char * start = s;
		while( *s && *s != ' ' )
			s++;

		int len = s - start;
		if( len >= MAX_QPATH || len < 5 ) {
			Com_Error( ERR_DROP, "PrecacheItem: %d has bad precache string", it->tag );
			return;
		}
		memcpy( data, start, len );
		data[len] = 0;
		if( *s ) {
			s++;
		}

		precache( data );
	}
}

void G_PrecacheItems() {
	for( int i = 0; i < Weapon_Count; i++ ) {
		const WeaponDef * weapon = GS_GetWeaponDef( i );
		PrecacheAssets( weapon->precache_models, trap_ModelIndex );
		PrecacheAssets( weapon->precache_sounds, trap_SoundIndex );
		PrecacheAssets( weapon->precache_images, trap_ImageIndex );
	}

	for( int i = 0; i < Item_Count; i++ ) {
		const Item * item = GS_FindItemByTag( i );
		PrecacheAssets( item->precache_models, trap_ModelIndex );
		PrecacheAssets( item->precache_sounds, trap_SoundIndex );
		PrecacheAssets( item->precache_images, trap_ImageIndex );
	}
}
