const int NUM_WEAPONS = 3;

const WeaponType[] primaries = { Weapon_Sniper, Weapon_RocketLauncher, Weapon_Laser };
const WeaponType[] secondaries = { Weapon_Rifle, Weapon_Shotgun, Weapon_Deagle, Weapon_Railgun, Weapon_GrenadeLauncher };
const WeaponType[] backup = { Weapon_MachineGun, Weapon_Pistol, Weapon_Plasma, Weapon_BubbleGun };


cPlayer@[] players( maxClients ); // array of handles
bool playersInitialized = false;

class cPlayer {
	Client @client;

	uint[] loadout( NUM_WEAPONS );
	int num_weapons;

	int killsThisRound;

	uint arms;
	uint defuses;

	bool dueToSpawn; // used for respawning during countdown

	cPlayer( Client @player ) {
		@this.client = @player;

		setLoadout( this.client.getUserInfoKey( "cg_loadout" ) );

		this.arms = 0;
		this.defuses = 0;

		this.dueToSpawn = false;

		@players[player.playerNum] = @this;
	}

	void rearrangeInventory() {
		for( int i = 0; i < this.num_weapons; i++ ) {
			this.client.setWeaponIndex( WeaponType( this.loadout[ i ] ), i + 1 ); // + 1 because of knife
		}
	}

	void giveInventory() {
		this.client.inventoryClear();
		this.client.giveWeapon( Weapon_Knife );

		for( int i = 0; i < this.num_weapons; i++ ) {
			this.client.giveWeapon( WeaponType( this.loadout[ i ] ) );
		}

		this.client.selectWeapon( 0 );
		this.client.selectWeapon( 1 );
	}

	void showShop() {
		if( this.client.team == TEAM_SPECTATOR ) {
			return;
		}

		String command = "changeloadout";
		for( int i = 0; i < this.num_weapons; i++ ) {
			command += " " + this.loadout[ i ];
		}
		this.client.execGameCommand( command );
	}

	void setLoadout( String &cmd ) {		
		WeaponType[][] category = { primaries, secondaries, backup };

		uint[] oldloadout = this.loadout;
		uint[] newloadout( NUM_WEAPONS );

		//Retrieve weapons
		this.num_weapons = 0;
		for( int i = 0; i < NUM_WEAPONS; i++ ) {
			String token = cmd.getToken( i );

			if( token == "" )
				break;

			int weapon = token.toInt();
			if( weapon > Weapon_None && weapon < Weapon_Count && weapon != Weapon_Knife ) {
				newloadout[ i ] = weapon;
				this.num_weapons++;
			}
		}


		//Check for categories
		for( uint i = 0; i < category.length(); i++ ) {
			for( uint j = 0; j < newloadout.length(); j++ ) {
				if( category[ i ].find( WeaponType( newloadout[ j ] ) ) != -1 ) {
					this.loadout[ j ] = newloadout[ j ];
					break;
				}
			}
		}

		String command = "saveloadout";
		for( int i = 0; i < this.num_weapons; i++ ) {
			command += " " + this.loadout[ i ];
		}
		this.client.execGameCommand( command );

		if( this.client.getEnt().isGhosting() ) {
			return;
		}

		if( match.getState() == MATCH_STATE_WARMUP || match.getState() == MATCH_STATE_COUNTDOWN ) {
			giveInventory();
		}

		if( match.getState() == MATCH_STATE_PLAYTIME ) {
			if( roundState == RoundState_Pre ) {
				giveInventory();
			}
			else {
				bool rearranging = true;

				for( uint i = 0; i < this.loadout.length(); i++ ) {
					if( this.loadout.find( oldloadout[ i ] ) == -1 ) {
						rearranging = false;
						break;
					}
				}

				if( rearranging ) {
					rearrangeInventory();
				}
			}
		}
	}
}

// since i am using an array of handles this must
// be done to avoid null references if there are players
// already on the server
void playersInit() {
	// do initial setup (that doesn't spawn any entities, but needs clients to be created) only once, not every round
	if( !playersInitialized ) {
		for( int i = 0; i < maxClients; i++ ) {
			Client @client = @G_GetClient( i );

			if( client.state() >= CS_CONNECTING ) {
				cPlayer( @client );
			}
		}

		playersInitialized = true;
	}
}

void resetKillCounters() {
	for( int i = 0; i < maxClients; i++ ) {
		if( @players[i] != null ) {
			players[i].killsThisRound = 0;
		}
	}
}

cPlayer @playerFromClient( Client @client ) {
	cPlayer @player = @players[client.playerNum];

	// XXX: as of 0.18 this check shouldn't be needed as playersInit works
	if( @player == null ) {
		assert( false, "player.as playerFromClient: no player exists for client - state: " + client.state() );

		return cPlayer( @client );
	}

	return @player;
}

void dropSpawnToFloor( Entity @ent ) {
	Vec3 mins( -16, -16, -24 ), maxs( 16, 16, 40 );

	Vec3 start = ent.origin + Vec3( 0, 0, 16 );
	Vec3 end = ent.origin - Vec3( 0, 0, 1024 );

	Trace trace;
	trace.doTrace( start, mins, maxs, end, ent.entNum, MASK_SOLID );

	if( trace.startSolid ) {
		G_Print( ent.classname + " starts inside solid, removing...\n" );
		ent.freeEntity();
		return;
	}

	// move it 1 unit away from the plane
	ent.origin = trace.endPos + trace.planeNormal;
}

void spawn_bomb_attacking( Entity @ent ) {
	dropSpawnToFloor( ent );
}

void spawn_bomb_defending( Entity @ent ) {
	dropSpawnToFloor( ent );
}

void team_CTF_alphaspawn( Entity @ent ) {
	dropSpawnToFloor( ent );
}

void team_CTF_betaspawn( Entity @ent ) {
	dropSpawnToFloor( ent );
}
