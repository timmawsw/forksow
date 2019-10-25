#include "qcommon/base.h"

const Item itemdefs[] = {
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
