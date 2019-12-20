#include "qcommon/base.h"

#define INSTANT 0

#define WEAPONDOWN_FRAMETIME 50
#define WEAPONUP_FRAMETIME 50

Weapon gs_weaponDefs[] = {
	{
		"Knife", "gb",
		RGB8( 255, 255, 255 ),
		"Knife people in the face",
		0,
		0,

		NULL, NULL, NULL,

		{
			AMMO_NONE,
			0,                              // ammo usage per shot
			0,                              // projectiles fired each shot

			//timings (in msecs)
			WEAPONUP_FRAMETIME,             // weapon up frametime
			WEAPONDOWN_FRAMETIME,           // weapon down frametime
			450,                            // reload frametime
			62,                             // projectile timeout  / projectile range for instant weapons
			false,                          // smooth refire

			//damages
			20,                             // damage
			0,                              // selfdamage ratio
			80,                             // knockback
			0,                              // splash radius
			0,                              // splash minimum damage
			0,                              // splash minimum knockback

			//projectile def
			INSTANT,                        // speed
			0,                              // spread
		},
	},

	{
		"SMG", "mg",
		RGB8( 254, 235, 98 ),
		"Shoots fast direct bullets touching enemies at any range",
		100,
		30,

		NULL, NULL, NULL,

		{
			AMMO_BULLETS,
			1,                              // ammo usage per shot
			1,                              // projectiles fired each shot

			//timings (in msecs)
			WEAPONUP_FRAMETIME,             // weapon up frametime
			WEAPONDOWN_FRAMETIME,           // weapon down frametime
			80,                            // reload frametime
			6000,                           // projectile timeout
			false,                          // smooth refire

			//damages
			4,                             // damage
			0,                              // selfdamage ratio
			40,                             // knockback
			0,                              // splash radius
			0,                              // splash minimum damage
			0,                              // splash minimum knockback

			//projectile def
			INSTANT,                        // speed
			0,                              // spread
		},
	},

	{
		"Shotgun", "rg",
		RGB8( 255, 172, 30 ),
		"Basically a shotgun",
		100,
		6,

		NULL, NULL, NULL,

		{
			AMMO_SHELLS,
			1,                              // ammo usage per shot
			20,                             // projectiles fired each shot

			//timings (in msecs)
			WEAPONUP_FRAMETIME,             // weapon up frametime
			WEAPONDOWN_FRAMETIME,           // weapon down frametime
			1000,                           // reload frametime
			8192,                           // projectile timeout / projectile range for instant weapons
			false,                          // smooth refire

			//damages
			2,                              // damage
			0,                              // selfdamage ratio (rg cant selfdamage)
			7,                              // knockback
			0,                              // splash radius
			0,                              // splash minimum damage
			0,                              // splash minimum knockback

			//projectile def
			INSTANT,                        // speed
			80,                             // spread
		},
	},

	{
		"Grenades", "gl",
		RGB8( 62, 141, 255 ),
		"Deprecated gun, enjoy it while it lasts nerds",
		100,
		3,

		PATH_GRENADE_MODEL,
		NULL, NULL,

		{
			AMMO_GRENADES,
			1,                              // ammo usage per shot
			1,                              // projectiles fired each shot

			//timings (in msecs)
			WEAPONUP_FRAMETIME,             // weapon up frametime
			WEAPONDOWN_FRAMETIME,           // weapon down frametime
			800,                            // reload frametime
			1250,                           // projectile timeout
			false,                          // smooth refire

			//damages
			40,                             // damage
			0.75,                           // selfdamage ratio
			100,                            // knockback
			125,                            // splash radius
			7,                              // splash minimum damage
			35,                             // splash minimum knockback

			//projectile def
			1000,                           // speed
			0,                              // spread
		},
	},

	{
		"Rockets", "rl",
		RGB8( 255, 58, 66 ),
		"Shoots slow moving rockets that deal damage in an area and push bodies away",
		200,
		5,

		PATH_ROCKET_MODEL,
		S_WEAPON_ROCKET_FLY,
		NULL,

		{
			AMMO_ROCKETS,
			1,                              // ammo usage per shot
			1,                              // projectiles fired each shot

			//timings (in msecs)
			WEAPONUP_FRAMETIME,             // weapon up frametime
			WEAPONDOWN_FRAMETIME,           // weapon down frametime
			1000,                           // reload frametime
			10000,                          // projectile timeout
			false,                          // smooth refire

			//damages
			40,                             // damage
			0.75,                           // selfdamage ratio
			100,                            // knockback
			120,                            // splash radius
			7,                              // splash minimum damage
			45,                             // splash minimum knockback

			//projectile def
			1250,                           // speed
			0,                              // spread
		},
	},

	{
		"Plasma", "pg",
		RGB8( 172, 80, 255 ),
		"Shoots fast projectiles that deal damage in an area",
		100,
		30,

		PATH_PLASMA_MODEL,
		S_WEAPON_PLASMAGUN_FLY,
		NULL,

		{
			AMMO_PLASMA,
			1,                              // ammo usage per shot
			1,                              // projectiles fired each shot

			//timings (in msecs)
			WEAPONUP_FRAMETIME,             // weapon up frametime
			WEAPONDOWN_FRAMETIME,           // weapon down frametime
			100,                            // reload frametime
			5000,                           // projectile timeout
			false,                          // smooth refire

			//damages
			8,                              // damage
			0.2,                            // selfdamage ratio
			22,                             // knockback
			45,                             // splash radius
			4,                              // splash minimum damage
			1,                              // splash minimum knockback

			//projectile def
			2500,                           // speed
			0,                              // spread
		},
	},

	{
		"Laser", "lg",
		RGB8( 82, 252, 95 ),
		"Shoots a continuous trail doing quick but low damage at a certain range",
		200,
		50,

		NULL,
		S_WEAPON_LASERGUN_HUM " "
			S_WEAPON_LASERGUN_STOP " "
			S_WEAPON_LASERGUN_HIT_0 " " S_WEAPON_LASERGUN_HIT_1 " " S_WEAPON_LASERGUN_HIT_2,
		NULL,

		{
			AMMO_LASERS,
			1,                              // ammo usage per shot
			1,                              // projectiles fired each shot

			//timings (in msecs)
			WEAPONUP_FRAMETIME,             // weapon up frametime
			WEAPONDOWN_FRAMETIME,           // weapon down frametime
			50,                             // reload frametime
			700,                            // projectile timeout / projectile range for instant weapons
			true,                           // smooth refire

			//damages
			4,                              // damage
			0,                              // selfdamage ratio (lg cant damage)
			14,                             // knockback
			0,                              // splash radius
			0,                              // splash minimum damage
			0,                              // splash minimum knockback

			//projectile def
			INSTANT,                        // speed
			0,                              // spread
		},
	},

	{
		"Railgun", "eb",
		RGB8( 80, 243, 255 ),
		"Shoots a direct laser hit doing pretty high damage",
		200,
		3,

		NULL,
		S_WEAPON_ELECTROBOLT_HIT,
		NULL,

		{
			AMMO_BOLTS,
			1,                              // ammo usage per shot
			1,                              // projectiles fired each shot

			//timings (in msecs)
			WEAPONUP_FRAMETIME,             // weapon up frametime
			WEAPONDOWN_FRAMETIME,           // weapon down frametime
			1250,                           // reload frametime
			900,                            // min damage range
			false,                          // smooth refire

			//damages
			35,                             // damage
			0,                              // selfdamage ratio
			80,                             // knockback
			0,                              // splash radius
			70,                             // minimum damage
			35,                             // minimum knockback

			//projectile def
			INSTANT,                        // speed
			0,                              // spread
		},
	},
};

STATIC_ASSERT( ARRAY_COUNT( gs_weaponDefs ) == Item_WeaponCount );

Weapon * GS_GetWeaponDef( int weapon ) {
	assert( weapon >= 0 && weapon < WEAP_TOTAL );
	return &gs_weaponDefs[ weapon ];
}
