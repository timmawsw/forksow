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
#include "qcommon/cmodel.h"

/*
==============================================================================

ZONE MEMORY ALLOCATION

There is never any space between memblocks, and there will never be two
contiguous free memblocks.

The rover can be left pointing at a non-empty block.

Ported over from Quake 1 and Quake 3.
==============================================================================
*/

#define TAG_FREE    0
#define TAG_LEVEL   1

#define ZONEID      0x1d4a11
#define MINFRAGMENT 64

typedef struct memblock_s
{
	int size;               // including the header and possibly tiny fragments
	int tag;                // a tag of 0 is a free block
	struct memblock_s       *next, *prev;
	int id;                 // should be ZONEID
} memblock_t;

typedef struct
{
	int size;           // total bytes malloced, including header
	int count, used;
	memblock_t blocklist;       // start / end cap for linked list
	memblock_t  *rover;
} memzone_t;

static memzone_t *levelzone;

/*
* G_Z_ClearZone
*/
static void G_Z_ClearZone( memzone_t *zone, int size ) {
	memblock_t  *block;

	// set the entire zone to one free block
	zone->blocklist.next = zone->blocklist.prev = block =
													  (memblock_t *)( (uint8_t *)zone + sizeof( memzone_t ) );
	zone->blocklist.tag = 1;    // in use block
	zone->blocklist.id = 0;
	zone->blocklist.size = 0;
	zone->size = size;
	zone->rover = block;
	zone->used = 0;
	zone->count = 0;

	block->prev = block->next = &zone->blocklist;
	block->tag = 0;         // free block
	block->id = ZONEID;
	block->size = size - sizeof( memzone_t );
}

/*
* G_Z_Free
*/
static void G_Z_Free( void *ptr, const char *filename, int fileline ) {
	memblock_t *block, *other;
	memzone_t *zone;

	if( !ptr ) {
		Com_Error( ERR_DROP, "G_Z_Free: NULL pointer" );
	}

	block = (memblock_t *) ( (uint8_t *)ptr - sizeof( memblock_t ) );
	if( block->id != ZONEID ) {
		Com_Error( ERR_DROP, "G_Z_Free: freed a pointer without ZONEID (file %s at line %i)", filename, fileline );
	}
	if( block->tag == 0 ) {
		Com_Error( ERR_DROP, "G_Z_Free: freed a freed pointer (file %s at line %i)", filename, fileline );
	}

	// check the memory trash tester
	if( *(int *)( (uint8_t *)block + block->size - 4 ) != ZONEID ) {
		Com_Error( ERR_DROP, "G_Z_Free: memory block wrote past end" );
	}

	zone = levelzone;
	zone->used -= block->size;
	zone->count--;

	block->tag = 0;     // mark as free

	other = block->prev;
	if( !other->tag ) {
		// merge with previous free block
		other->size += block->size;
		other->next = block->next;
		other->next->prev = other;
		if( block == zone->rover ) {
			zone->rover = other;
		}
		block = other;
	}

	other = block->next;
	if( !other->tag ) {
		// merge the next free block onto the end
		block->size += other->size;
		block->next = other->next;
		block->next->prev = block;
		if( other == zone->rover ) {
			zone->rover = block;
		}
	}
}

/*
* G_Z_TagMalloc
*/
static void *G_Z_TagMalloc( int size, int tag, const char *filename, int fileline ) {
	int extra;
	memblock_t *start, *rover, *newb, *base;
	memzone_t *zone;

	if( !tag ) {
		Com_Error( ERR_DROP, "G_Z_TagMalloc: tried to use a 0 tag (file %s at line %i)", filename, fileline );
	}

	//
	// scan through the block list looking for the first free block
	// of sufficient size
	//
	size += sizeof( memblock_t ); // account for size of block header
	size += 4;                  // space for memory trash tester
	size = ( size + 3 ) & ~3;     // align to 32-bit boundary

	zone = levelzone;
	base = rover = zone->rover;
	start = base->prev;

	do {
		if( rover == start ) {  // scaned all the way around the list
			return NULL;
		}
		if( rover->tag ) {
			base = rover = rover->next;
		} else {
			rover = rover->next;
		}
	} while( base->tag || base->size < size );

	//
	// found a block big enough
	//
	extra = base->size - size;
	if( extra > MINFRAGMENT ) {
		// there will be a free fragment after the allocated block
		newb = (memblock_t *) ( (uint8_t *)base + size );
		newb->size = extra;
		newb->tag = 0;          // free block
		newb->prev = base;
		newb->id = ZONEID;
		newb->next = base->next;
		newb->next->prev = newb;
		base->next = newb;
		base->size = size;
	}

	base->tag = tag;                // no longer a free block
	zone->rover = base->next;   // next allocation will start looking here
	zone->used += base->size;
	zone->count++;
	base->id = ZONEID;

	// marker for memory trash testing
	*(int *)( (uint8_t *)base + base->size - 4 ) = ZONEID;

	return (void *) ( (uint8_t *)base + sizeof( memblock_t ) );
}

/*
* G_Z_Malloc
*/
static void *G_Z_Malloc( int size, const char *filename, int fileline ) {
	void    *buf;

	buf = G_Z_TagMalloc( size, TAG_LEVEL, filename, fileline );
	if( !buf ) {
		Com_Error( ERR_DROP, "G_Z_Malloc: failed on allocation of %i bytes", size );
	}
	memset( buf, 0, size );

	return buf;
}

//==============================================================================

/*
* G_LevelInitPool
*/
void G_LevelInitPool( size_t size ) {
	G_LevelFreePool();

	levelzone = ( memzone_t * )G_Malloc( size );
	G_Z_ClearZone( levelzone, size );
}

/*
* G_LevelFreePool
*/
void G_LevelFreePool() {
	if( levelzone ) {
		G_Free( levelzone );
		levelzone = NULL;
	}
}

/*
* G_LevelMalloc
*/
void *_G_LevelMalloc( size_t size, const char *filename, int fileline ) {
	return G_Z_Malloc( size, filename, fileline );
}

/*
* G_LevelFree
*/
void _G_LevelFree( void *data, const char *filename, int fileline ) {
	G_Z_Free( data, filename, fileline );
}

/*
* G_LevelCopyString
*/
char *_G_LevelCopyString( const char *in, const char *filename, int fileline ) {
	char *out;

	out = ( char * )_G_LevelMalloc( strlen( in ) + 1, filename, fileline );
	strcpy( out, in );
	return out;
}

//==============================================================================

#define STRINGPOOL_SIZE         1024 * 1024
#define STRINGPOOL_HASH_SIZE    32

struct g_poolstring_t {
	char *buf;
	g_poolstring_t *hash_next;
};

static uint8_t *g_stringpool;
static size_t g_stringpool_offset;
static g_poolstring_t *g_stringpool_hash[STRINGPOOL_HASH_SIZE];

/*
* G_StringPoolInit
*
* Preallocates a memory region to permanently store level strings
*/
void G_StringPoolInit() {
	memset( g_stringpool_hash, 0, sizeof( g_stringpool_hash ) );

	g_stringpool = ( uint8_t * )G_LevelMalloc( STRINGPOOL_SIZE );
	g_stringpool_offset = 0;
}

/*
* G_StringPoolHashKey
*/
static unsigned int G_StringPoolHashKey( const char *string ) {
	int i;
	unsigned int v;
	unsigned int c;

	v = 0;
	for( i = 0; string[i]; i++ ) {
		c = string[i];
		v = ( v + i ) * 37 + c;
	}

	return v % STRINGPOOL_HASH_SIZE;
}

/*
* G_RegisterLevelString
*
* Registers a unique string which is guaranteed to exist until the level reloads
*/
const char *_G_RegisterLevelString( const char *string, const char *filename, int fileline ) {
	size_t size;
	g_poolstring_t *ps;
	unsigned int hashkey;

	if( !string ) {
		return NULL;
	}
	if( !*string ) {
		return "";
	}

	size = strlen( string ) + 1;
	if( sizeof( *ps ) + size > STRINGPOOL_SIZE ) {
		Com_Error( ERR_DROP, "G_RegisterLevelString: out of memory (str:%s at %s:%i)\n", string, filename, fileline );
		return NULL;
	}

	// find a matching registered string
	hashkey = G_StringPoolHashKey( string );
	for( ps = g_stringpool_hash[hashkey]; ps; ps = ps->hash_next ) {
		if( !strcmp( ps->buf, string ) ) {
			return ps->buf;
		}
	}

	// no match, register a new one
	ps = ( g_poolstring_t * )( g_stringpool + g_stringpool_offset );
	g_stringpool_offset += sizeof( *ps );

	ps->buf = ( char * )( g_stringpool + g_stringpool_offset );
	ps->hash_next = g_stringpool_hash[hashkey];
	g_stringpool_hash[hashkey] = ps;

	memcpy( ps->buf, string, size );
	g_stringpool_offset += size;

	return ps->buf;
}

/*
* G_Find
*
* Searches all active entities for the next one that holds
* the matching string at fieldofs (use the FOFS() macro) in the structure.
*
* Searches beginning at the edict after from, or the beginning if NULL
* NULL will be returned if the end of the list is reached.
*
*/
edict_t *G_Find( edict_t *from, size_t fieldofs, const char *match ) {
	char *s;

	if( !from ) {
		from = world;
	} else {
		from++;
	}

	for(; from <= &game.edicts[game.numentities - 1]; from++ ) {
		if( !from->r.inuse ) {
			continue;
		}
		s = *(char **) ( (uint8_t *)from + fieldofs );
		if( !s ) {
			continue;
		}
		if( !Q_stricmp( s, match ) ) {
			return from;
		}
	}

	return NULL;
}

/*
* G_PickTarget
*
* Searches all active entities for the next one that holds
* the matching string at fieldofs (use the FOFS() macro) in the structure.
*
* Searches beginning at the edict after from, or the beginning if NULL
* NULL will be returned if the end of the list is reached.
*
*/
#define MAXCHOICES  8

edict_t *G_PickTarget( const char *targetname ) {
	edict_t *ent = NULL;
	int num_choices = 0;
	edict_t *choice[MAXCHOICES];

	if( !targetname ) {
		Com_Printf( "G_PickTarget called with NULL targetname\n" );
		return NULL;
	}

	while( 1 ) {
		ent = G_Find( ent, FOFS( targetname ), targetname );
		if( !ent ) {
			break;
		}
		choice[num_choices++] = ent;
		if( num_choices == MAXCHOICES ) {
			break;
		}
	}

	if( !num_choices ) {
		Com_Printf( "G_PickTarget: target %s not found\n", targetname );
		return NULL;
	}

	return choice[ random_uniform( &svs.rng, 0, num_choices ) ];
}



static void Think_Delay( edict_t *ent ) {
	G_UseTargets( ent, ent->activator );
	G_FreeEdict( ent );
}

/*
* G_UseTargets
*
* the global "activator" should be set to the entity that initiated the firing.
*
* If self.delay is set, a DelayedUse entity will be created that will actually
* do the SUB_UseTargets after that many seconds have passed.
*
* Centerprints any self.message to the activator.
*
* Search for (string)targetname in all entities that
* match (string)self.target and call their .use function
*
*/
void G_UseTargets( edict_t *ent, edict_t *activator ) {
	edict_t *t;

	//
	// check for a delay
	//
	if( ent->delay ) {
		// create a temp object to fire at a later time
		t = G_Spawn();
		t->classname = "delayed_use";
		t->nextThink = level.time + 1000 * ent->delay;
		t->think = Think_Delay;
		t->activator = activator;
		if( !activator ) {
			Com_Printf( "Think_Delay with no activator\n" );
		}
		t->message = ent->message;
		t->target = ent->target;
		t->killtarget = ent->killtarget;
		return;
	}

	//
	// print the message
	//
	if( ent->message ) {
		G_CenterPrintMsg( activator, "%s", ent->message );

		if( ent->sound != EMPTY_HASH ) {
			G_Sound( activator, CHAN_AUTO, ent->sound );
		}
	}

	//
	// kill killtargets
	//
	if( ent->killtarget ) {
		t = NULL;
		while( ( t = G_Find( t, FOFS( targetname ), ent->killtarget ) ) ) {
			G_FreeEdict( t );
			if( !ent->r.inuse ) {
				Com_Printf( "entity was removed while using killtargets\n" );
				return;
			}
		}
	}

	//	Com_Printf ("TARGET: activating %s\n", ent->target);

	//
	// fire targets
	//
	if( ent->target ) {
		t = NULL;
		while( ( t = G_Find( t, FOFS( targetname ), ent->target ) ) ) {
			if( t == ent ) {
				Com_Printf( "WARNING: Entity used itself.\n" );
			} else {
				G_CallUse( t, ent, activator );
			}
			if( !ent->r.inuse ) {
				Com_Printf( "entity was removed while using targets\n" );
				return;
			}
		}
	}
}

void G_SetMovedir( Vec3 * angles, Vec3 * movedir ) {
	AngleVectors( *angles, movedir, NULL, NULL );
	*angles = Vec3( 0.0f );
}

char *_G_CopyString( const char *in, const char *filename, int fileline ) {
	char * out = ( char * )_Mem_AllocExt( gamepool, strlen( in ) + 1, 16, 1, 0, 0, filename, fileline );
	strcpy( out, in );
	return out;
}

/*
* G_FreeEdict
*
* Marks the edict as free
*/
void G_FreeEdict( edict_t *ed ) {
	bool evt = ISEVENTENTITY( &ed->s );

	GClip_UnlinkEntity( ed );   // unlink from world

	G_asReleaseEntityBehaviors( ed );

	memset( ed, 0, sizeof( *ed ) );
	ed->r.inuse = false;
	ed->s.number = ENTNUM( ed );
	ed->r.svflags = SVF_NOCLIENT;
	ed->scriptSpawned = false;

	if( !evt && ( level.spawnedTimeStamp != svs.realtime ) ) {
		ed->freetime = svs.realtime; // ET_EVENT or ET_SOUND don't need to wait to be reused
	}
}

/*
* G_InitEdict
*/
void G_InitEdict( edict_t *e ) {
	e->r.inuse = true;
	e->classname = NULL;
	e->timeDelta = 0;
	e->deadflag = DEAD_NO;
	e->timeStamp = 0;
	e->scriptSpawned = false;

	memset( &e->s, 0, sizeof( SyncEntityState ) );
	e->s.number = ENTNUM( e );

	G_asClearEntityBehaviors( e );

	// mark all entities to not be sent by default
	e->r.svflags = SVF_NOCLIENT | (e->r.svflags & SVF_FAKECLIENT);

	// clear the old state data
	memset( &e->olds, 0, sizeof( e->olds ) );
	memset( &e->snap, 0, sizeof( e->snap ) );
}

/*
* G_Spawn
*
* Either finds a free edict, or allocates a new one.
* Try to avoid reusing an entity that was recently freed, because it
* can cause the client to think the entity morphed into something else
* instead of being removed and recreated, which can cause interpolated
* angles and bad trails.
*/
edict_t *G_Spawn() {
	if( !level.canSpawnEntities ) {
		Com_Printf( "WARNING: Spawning entity before map entities have been spawned\n" );
	}

	int i;
	edict_t * freed = NULL;
	edict_t * e = &game.edicts[server_gs.maxclients + 1];
	for( i = server_gs.maxclients + 1; i < game.numentities; i++, e++ ) {
		if( e->r.inuse ) {
			continue;
		}

		// the first couple seconds of server time can involve a lot of
		// freeing and allocating, so relax the replacement policy
		if( e->freetime < level.spawnedTimeStamp + 2000 || svs.realtime > e->freetime + 500 ) {
			G_InitEdict( e );
			return e;
		}

		// this is going to be our second chance to spawn an entity in case all free
		// entities have been freed only recently
		if( !freed ) {
			freed = e;
		}
	}

	if( i == game.maxentities ) {
		if( freed ) {
			G_InitEdict( freed );
			return freed;
		}
		Com_Error( ERR_DROP, "G_Spawn: no free edicts" );
	}

	game.numentities++;

	SV_LocateEntities( game.edicts, game.numentities, game.maxentities );

	G_InitEdict( e );

	return e;
}

/*
* G_AddEvent
*/
void G_AddEvent( edict_t *ent, int event, u64 parm, bool highPriority ) {
	if( !ent || ent == world || !ent->r.inuse ) {
		return;
	}
	if( !event ) {
		return;
	}

	int eventNum = ent->numEvents & 1;
	if( ent->eventPriority[eventNum] && !ent->eventPriority[( eventNum + 1 ) & 1] ) {
		eventNum = ( eventNum + 1 ) & 1; // prefer overwriting low priority events
	} else if( !highPriority && ent->eventPriority[eventNum] ) {
		return; // no low priority event to overwrite
	} else {
		ent->numEvents++; // numEvents is only used to vary the overwritten event

	}
	ent->s.events[eventNum].type = event;
	ent->s.events[eventNum].parm = parm;
	ent->eventPriority[eventNum] = highPriority;
}

/*
* G_SpawnEvent
*/
edict_t *G_SpawnEvent( int event, u64 parm, const Vec3 * origin ) {
	edict_t * ent = G_Spawn();
	ent->s.type = ET_EVENT;
	ent->r.solid = SOLID_NOT;
	ent->r.svflags &= ~SVF_NOCLIENT;
	if( origin ) {
		ent->s.origin = *origin;
	}
	G_AddEvent( ent, event, parm, true );

	GClip_LinkEntity( ent );

	return ent;
}

/*
* G_MorphEntityIntoEvent
*/
void G_MorphEntityIntoEvent( edict_t *ent, int event, u64 parm ) {
	ent->s.type = ET_EVENT;
	ent->r.solid = SOLID_NOT;
	ent->r.svflags &= ~SVF_PROJECTILE; // FIXME: Medar: should be remove all or remove this one elsewhere?
	ent->s.linearMovement = false;
	G_AddEvent( ent, event, parm, true );

	GClip_LinkEntity( ent );
}

/*
* G_InitMover
*/
void G_InitMover( edict_t *ent ) {
	ent->r.solid = SOLID_YES;
	ent->movetype = MOVETYPE_PUSH;
	ent->r.svflags &= ~SVF_NOCLIENT;

	GClip_SetBrushModel( ent );
}

/*
* G_CallThink
*/
void G_CallThink( edict_t *ent ) {
	if( ent->think ) {
		ent->think( ent );
	} else if( ent->scriptSpawned && ent->asThinkFunc ) {
		G_asCallMapEntityThink( ent );
	} else if( developer->integer ) {
		Com_Printf( "NULL ent->think in %s\n", ent->classname ? ent->classname : va( "'no classname. Entity type is %i", ent->s.type ) );
	}
}

/*
* G_CallTouch
*/
void G_CallTouch( edict_t *self, edict_t *other, cplane_t *plane, int surfFlags ) {
	if( self == other ) {
		return;
	}

	if( self->touch ) {
		self->touch( self, other, plane, surfFlags );
	} else if( self->scriptSpawned && self->asTouchFunc ) {
		G_asCallMapEntityTouch( self, other, plane, surfFlags );
	}
}

/*
* G_CallUse
*/
void G_CallUse( edict_t *self, edict_t *other, edict_t *activator ) {
	if( self->use ) {
		self->use( self, other, activator );
	} else if( self->scriptSpawned && self->asUseFunc ) {
		G_asCallMapEntityUse( self, other, activator );
	}
}

/*
* G_CallStop
*/
void G_CallStop( edict_t *self ) {
	if( self->stop ) {
		self->stop( self );
	} else if( self->scriptSpawned && self->asStopFunc ) {
		G_asCallMapEntityStop( self );
	}
}

/*
* G_CallPain
*/
void G_CallPain( edict_t *ent, edict_t *attacker, float kick, float damage ) {
	if( ent->pain ) {
		ent->pain( ent, attacker, kick, damage );
	} else if( ent->scriptSpawned && ent->asPainFunc ) {
		G_asCallMapEntityPain( ent, attacker, kick, damage );
	}
}

/*
* G_CallDie
*/
void G_CallDie( edict_t *ent, edict_t *inflictor, edict_t *attacker, int assistorNo, int damage, Vec3 point ) {
	if( ent->die ) {
		ent->die( ent, inflictor, attacker, assistorNo, damage, point );
	} else if( ent->scriptSpawned && ent->asDieFunc ) {
		G_asCallMapEntityDie( ent, inflictor, attacker, damage, point );
	}
}


/*
* G_PrintMsg
*
* NULL sends to all the message to all clients
*/
void G_PrintMsg( edict_t *ent, const char *format, ... ) {
	char msg[MAX_STRING_CHARS];
	va_list argptr;
	char *s, *p;

	va_start( argptr, format );
	vsnprintf( msg, sizeof( msg ), format, argptr );
	va_end( argptr );

	// double quotes are bad
	p = msg;
	while( ( p = strchr( p, '\"' ) ) != NULL )
		*p = '\'';

	s = va( "pr \"%s\"", msg );

	if( !ent ) {
		// mirror at server console
		if( is_dedicated_server ) {
			Com_Printf( "%s", msg );
		}
		PF_GameCmd( NULL, s );
	} else {
		if( ent->r.inuse && ent->r.client ) {
			PF_GameCmd( ent, s );
		}
	}
}

/*
* G_ChatMsg
*
* NULL sends the message to all clients
*/
void G_ChatMsg( edict_t *ent, edict_t *who, bool teamonly, const char *format, ... ) {
	char msg[1024];
	va_list argptr;
	char *s, *p;

	va_start( argptr, format );
	vsnprintf( msg, sizeof( msg ), format, argptr );
	va_end( argptr );

	// double quotes are bad
	p = msg;
	while( ( p = strchr( p, '\"' ) ) != NULL )
		*p = '\'';

	s = va( "%s %i \"%s\"", ( who && teamonly ? "tch" : "ch" ), ( who ? ENTNUM( who ) : 0 ), msg );

	if( !ent ) {
		// mirror at server console
		if( is_dedicated_server ) {
			if( !who ) {
				Com_Printf( "Console: %s\n", msg );     // admin console
			} else if( !who->r.client ) {
				;   // wtf?
			} else if( teamonly ) {
				Com_Printf( "[%s] %s %s\n",
						  who->r.client->ps.team == TEAM_SPECTATOR ? "SPEC" : "TEAM", who->r.client->netname, msg );
			} else {
				Com_Printf( "%s: %s\n", who->r.client->netname, msg );
			}
		}

		if( who && teamonly ) {
			int i;

			for( i = 0; i < server_gs.maxclients; i++ ) {
				ent = game.edicts + 1 + i;

				if( ent->r.inuse && ent->r.client && PF_GetClientState( i ) >= CS_CONNECTED ) {
					if( ent->s.team == who->s.team ) {
						PF_GameCmd( ent, s );
					}
				}
			}
		} else {
			PF_GameCmd( NULL, s );
		}
	} else {
		if( ent->r.inuse && ent->r.client && PF_GetClientState( PLAYERNUM( ent ) ) >= CS_CONNECTED ) {
			if( !who || !teamonly || ent->s.team == who->s.team ) {
				PF_GameCmd( ent, s );
			}
		}
	}
}

/*
* G_CenterPrintMsg
*
* NULL sends to all the message to all clients
*/
void G_CenterPrintMsg( edict_t *ent, const char *format, ... ) {
	char msg[1024];
	char cmd[MAX_STRING_CHARS];
	va_list argptr;
	char *p;
	edict_t *other;

	va_start( argptr, format );
	vsnprintf( msg, sizeof( msg ), format, argptr );
	va_end( argptr );

	// double quotes are bad
	p = msg;
	while( ( p = strchr( p, '\"' ) ) != NULL )
		*p = '\'';

	snprintf( cmd, sizeof( cmd ), "cp \"%s\"", msg );
	PF_GameCmd( ent, cmd );

	if( ent != NULL ) {
		// add it to every player who's chasing this player
		for( other = game.edicts + 1; PLAYERNUM( other ) < server_gs.maxclients; other++ ) {
			if( !other->r.client || !other->r.inuse || !other->r.client->resp.chase.active ) {
				continue;
			}

			if( other->r.client->resp.chase.target == ENTNUM( ent ) ) {
				PF_GameCmd( other, cmd );
			}
		}
	}
}

void G_ClearCenterPrint( edict_t *ent ) {
	G_CenterPrintMsg( ent, "%s", "" );
}

void G_Obituary( edict_t * victim, edict_t * attacker, int topAssistEntNo, int mod, bool wallbang ) {
	TempAllocator temp = svs.frame_arena.temp();
	PF_GameCmd( NULL, temp( "obry {} {} {} {} {} {}", ENTNUM( victim ), ENTNUM( attacker ), topAssistEntNo, mod, wallbang ? 1 : 0, random_u64( &svs.rng ) ) );
}

//==================================================
// SOUNDS
//==================================================

static edict_t *_G_SpawnSound( int channel, StringHash sound ) {
	edict_t * ent = G_Spawn();
	ent->r.svflags &= ~SVF_NOCLIENT;
	ent->r.svflags |= SVF_SOUNDCULL;
	ent->s.type = ET_SOUNDEVENT;
	ent->s.channel = channel;
	ent->s.sound = sound;

	return ent;
}

edict_t *G_Sound( edict_t *owner, int channel, StringHash sound ) {
	if( sound == EMPTY_HASH ) {
		return NULL;
	}

	if( ISEVENTENTITY( &owner->s ) ) {
		return NULL; // event entities can't be owner of sound entities
	}

	edict_t * ent = _G_SpawnSound( channel, sound );
	ent->s.ownerNum = owner->s.number;

	const cmodel_t * cmodel = CM_TryFindCModel( CM_Server, owner->s.model );
	if( cmodel != NULL ) {
		ent->s.origin = owner->s.origin;
	}
	else {
		ent->s.origin = ( owner->r.absmin + owner->r.absmax ) * 0.5f;
	}

	GClip_LinkEntity( ent );
	return ent;
}

edict_t *G_PositionedSound( Vec3 origin, int channel, StringHash sound ) {
	if( sound == EMPTY_HASH ) {
		return NULL;
	}

	edict_t * ent = _G_SpawnSound( channel, sound );
	if( origin != Vec3( 0.0f ) ) {
		ent->s.channel |= CHAN_FIXED;
		ent->s.origin = origin;
	}
	else {
		ent->r.svflags |= SVF_BROADCAST;
	}

	GClip_LinkEntity( ent );
	return ent;
}

void G_GlobalSound( int channel, StringHash sound ) {
	G_PositionedSound( Vec3( 0.0f ), channel, sound );
}

void G_LocalSound( edict_t * owner, int channel, StringHash sound ) {
	if( sound == EMPTY_HASH )
		return;

	if( ISEVENTENTITY( &owner->s ) ) {
		return; // event entities can't be owner of sound entities
	}

	edict_t * ent = _G_SpawnSound( channel, sound );
	ent->s.ownerNum = ENTNUM( owner );
	ent->r.svflags |= SVF_ONLYOWNER | SVF_BROADCAST;

	GClip_LinkEntity( ent );
}

//==============================================================================
//
//Kill box
//
//==============================================================================

/*
* KillBox
*
* Kills all entities that would touch the proposed new positioning
* of ent.  Ent should be unlinked before calling this!
*/
bool KillBox( edict_t *ent, int mod, Vec3 knockback ) {
	trace_t tr;
	bool telefragged = false;

	while( 1 ) {
		G_Trace( &tr, ent->s.origin, ent->r.mins, ent->r.maxs, ent->s.origin, world, MASK_PLAYERSOLID );
		if( ( tr.fraction == 1.0f && !tr.startsolid ) || tr.ent < 0 ) {
			return telefragged;
		}

		if( tr.ent == ENTNUM( world ) ) {
			return telefragged; // found the world (but a player could be in there too). suicide?
		}

		// nail it
		G_Damage( &game.edicts[tr.ent], ent, ent, knockback, Vec3( 0.0f ), ent->s.origin, 100000, Length( knockback ), 0, mod );
		telefragged = true;

		// if we didn't kill it, fail
		if( game.edicts[tr.ent].r.solid ) {
			return telefragged;
		}
	}

	return telefragged; // all clear
}

/*
* LookAtKillerYAW
* returns the YAW angle to look at our killer
*/
float LookAtKillerYAW( edict_t *self, edict_t *inflictor, edict_t *attacker ) {
	Vec3 dir;

	if( attacker && attacker != world && attacker != self ) {
		dir = attacker->s.origin - self->s.origin;
	} else if( inflictor && inflictor != world && inflictor != self ) {
		dir = inflictor->s.origin - self->s.origin;
	} else {
		return self->s.angles.y;
	}

	return VecToAngles( dir ).y;
}

//==============================================================================
//
//		Warsow: more miscelanea tools
//
//==============================================================================

/*
* G_SpawnTeleportEffect
*/
static void G_SpawnTeleportEffect( edict_t *ent, bool respawn, bool in ) {
	edict_t *event;

	if( !ent || !ent->r.client ) {
		return;
	}

	if( PF_GetClientState( PLAYERNUM( ent ) ) < CS_SPAWNED || ent->r.solid == SOLID_NOT ) {
		return;
	}

	// add a teleportation effect
	event = G_SpawnEvent( respawn ? EV_PLAYER_RESPAWN : ( in ? EV_PLAYER_TELEPORT_IN : EV_PLAYER_TELEPORT_OUT ), 0, &ent->s.origin );
	event->s.ownerNum = ENTNUM( ent );
}

void G_TeleportEffect( edict_t *ent, bool in ) {
	G_SpawnTeleportEffect( ent, false, in );
}

void G_RespawnEffect( edict_t *ent ) {
	G_SpawnTeleportEffect( ent, true, false );
}

/*
* G_SolidMaskForEnt
*/
int G_SolidMaskForEnt( edict_t *ent ) {
	return ent->r.clipmask ? ent->r.clipmask : MASK_SOLID;
}

/*
* G_CheckEntGround
*/
void G_CheckGround( edict_t *ent ) {
	trace_t trace;

	if( ent->r.client && ent->velocity.z > 180 ) {
		ent->groundentity = NULL;
		ent->groundentity_linkcount = 0;
		return;
	}

	// if the hull point one-quarter unit down is solid the entity is on ground
	Vec3 point = ent->s.origin;
	point.z -= 0.25f;

	G_Trace( &trace, ent->s.origin, ent->r.mins, ent->r.maxs, point, ent, G_SolidMaskForEnt( ent ) );

	// check steepness
	if( !ISWALKABLEPLANE( &trace.plane ) && !trace.startsolid ) {
		ent->groundentity = NULL;
		ent->groundentity_linkcount = 0;
		return;
	}

	if( ent->velocity.z > 1.0f && !ent->r.client && !trace.startsolid ) {
		ent->groundentity = NULL;
		ent->groundentity_linkcount = 0;
		return;
	}

	if( !trace.startsolid && !trace.allsolid ) {
		//VectorCopy( trace.endpos, ent->s.origin );
		ent->groundentity = &game.edicts[trace.ent];
		ent->groundentity_linkcount = ent->groundentity->linkcount;
		if( ent->velocity.z < 0.0f ) {
			ent->velocity.z = 0.0f;
		}
	}
}

/*
* G_CategorizePosition
*/
void G_CategorizePosition( edict_t *ent ) {
	int cont;

	//
	// get waterlevel
	//
	Vec3 point = ent->s.origin;
	point.z += ent->r.mins.z + 1.0f;
	cont = G_PointContents( point );

	if( !( cont & MASK_WATER ) ) {
		ent->waterlevel = 0;
		ent->watertype = 0;
		return;
	}

	ent->watertype = cont;
	ent->waterlevel = 1;
	point.z += 26;
	cont = G_PointContents( point );
	if( !( cont & MASK_WATER ) ) {
		return;
	}

	ent->waterlevel = 2;
	point.z += 22;
	cont = G_PointContents( point );
	if( cont & MASK_WATER ) {
		ent->waterlevel = 3;
	}
}

/*
* G_SetBoundsForSpanEntity
*
* Set origin and origin2 and then call this before linkEntity
* for laser entities for proper clipping against world leafs/clusters.
*/
void G_SetBoundsForSpanEntity( edict_t *ent, float size ) {
	ClearBounds( &ent->r.absmin, &ent->r.absmax );
	AddPointToBounds( ent->s.origin, &ent->r.absmin, &ent->r.absmax );
	AddPointToBounds( ent->s.origin2, &ent->r.absmin, &ent->r.absmax );
	ent->r.absmin -= size;
	ent->r.absmax += size;
	ent->r.mins = ent->r.absmin - ent->s.origin;
	ent->r.maxs = ent->r.absmax - ent->s.origin;
}

/*
* G_ReleaseClientPSEvent
*/
void G_ReleaseClientPSEvent( gclient_t *client ) {
	for( int i = 0; i < 2; i++ ) {
		if( client->resp.eventsCurrent < client->resp.eventsHead ) {
			client->ps.events[ i ] = client->resp.events[client->resp.eventsCurrent & MAX_CLIENT_EVENTS_MASK];
			client->resp.eventsCurrent++;
		} else {
			client->ps.events[ i ] = { };
		}
	}
}

/*
* G_AddPlayerStateEvent
* This event is only sent to this client inside its SyncPlayerState.
*/
void G_AddPlayerStateEvent( gclient_t *client, int ev, u64 parm ) {
	assert( ev >= 0 && ev < PSEV_MAX_EVENTS );
	if( client == NULL )
		return;

	SyncEvent * event = &client->resp.events[client->resp.eventsHead & MAX_CLIENT_EVENTS_MASK];
	client->resp.eventsHead++;
	event->type = ev;
	event->parm = parm;
}

/*
* G_ClearPlayerStateEvents
*/
void G_ClearPlayerStateEvents( gclient_t *client ) {
	if( client ) {
		memset( client->resp.events, PSEV_NONE, sizeof( client->resp.events ) );
		client->resp.eventsCurrent = client->resp.eventsHead = 0;
	}
}

/*
* G_PlayerForText
* Returns player matching given text. It can be either number of the player or player's name.
*/
edict_t *G_PlayerForText( const char *text ) {
	if( !text || !text[0] ) {
		return NULL;
	}

	int pnum = atoi( text );

	if( !Q_stricmp( text, va( "%i", pnum ) ) && pnum >= 0 && pnum < server_gs.maxclients && game.edicts[pnum + 1].r.inuse ) {
		return &game.edicts[atoi( text ) + 1];
	}

	int i;
	edict_t *e;

	// check if it's a known player name
	for( i = 0, e = game.edicts + 1; i < server_gs.maxclients; i++, e++ ) {
		if( !e->r.inuse ) {
			continue;
		}

		if( !Q_stricmp( text, e->r.client->netname ) ) {
			return e;
		}
	}

	// nothing found
	return NULL;
}

void G_AnnouncerSound( edict_t *targ, StringHash sound, int team, bool queued, edict_t *ignore ) {
	int psev = queued ? PSEV_ANNOUNCER_QUEUED : PSEV_ANNOUNCER;
	int playerTeam;

	if( targ ) { // only for a given player
		if( !targ->r.client || PF_GetClientState( PLAYERNUM( targ ) ) < CS_SPAWNED ) {
			return;
		}

		if( targ == ignore ) {
			return;
		}

		G_AddPlayerStateEvent( targ->r.client, psev, sound.hash );
	} else {   // add it to all players
		edict_t *ent;

		for( ent = game.edicts + 1; PLAYERNUM( ent ) < server_gs.maxclients; ent++ ) {
			if( !ent->r.inuse || PF_GetClientState( PLAYERNUM( ent ) ) < CS_SPAWNED ) {
				continue;
			}

			if( ent == ignore ) {
				continue;
			}

			// team filter
			if( team >= TEAM_SPECTATOR && team < GS_MAX_TEAMS ) {
				playerTeam = ent->s.team;

				// if in chasecam, assume the player is in the chased player team
				if( playerTeam == TEAM_SPECTATOR && ent->r.client->resp.chase.active
					&& ent->r.client->resp.chase.target > 0 ) {
					playerTeam = game.edicts[ent->r.client->resp.chase.target].s.team;
				}

				if( playerTeam != team ) {
					continue;
				}
			}

			G_AddPlayerStateEvent( ent->r.client, psev, sound.hash );
		}
	}
}
