// Emacs style mode select   -*- C -*-
//-----------------------------------------------------------------------------
//
// Copyright(C) 1993-1997 Id Software, Inc.
// Copyright(C) 1997 Midway Home Entertainment, Inc
// Copyright(C) 2007-2012 Samuel Villarreal
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
// 02111-1307, USA.
//
//-----------------------------------------------------------------------------
//
// DESCRIPTION:
//    Enemy thinking, AI.
//    Action Pointer Functions
//    that are associated with states/frames.
//
//-----------------------------------------------------------------------------

#include <stdlib.h>

#include "m_random.h"
#include "m_fixed.h"
#include "i_system.h"
#include "doomdef.h"
#include "p_local.h"
#include "p_macros.h"
#include "s_sound.h"
#include "g_game.h"
#include "doomstat.h"
#include "r_local.h"
#include "sounds.h"
#include "tables.h"
#include "info.h"
#include "z_zone.h"

typedef enum {
	DI_EAST,
	DI_NORTHEAST,
	DI_NORTH,
	DI_NORTHWEST,
	DI_WEST,
	DI_SOUTHWEST,
	DI_SOUTH,
	DI_SOUTHEAST,
	DI_NODIR,
	NUMDIRS
} dirtype_t;

//
// P_NewChaseDir related LUT.
//
dirtype_t opposite[] = {
	DI_WEST, DI_SOUTHWEST, DI_SOUTH, DI_SOUTHEAST,
	DI_EAST, DI_NORTHEAST, DI_NORTH, DI_NORTHWEST,
	DI_NODIR
};

dirtype_t diags[] = {
	DI_NORTHWEST, DI_NORTHEAST, DI_SOUTHWEST, DI_SOUTHEAST
};

void A_Fall(mobj_t* actor);

//
// ENEMY THINKING
// Enemies are allways spawned
// with targetplayer = -1, threshold = 0
// Most monsters are spawned unaware of all players,
// but some can be made preaware
//

//
// P_RecursiveSound
//
// Called by P_NoiseAlert.
// Recursively traverse adjacent sectors,
// sound blocking lines cut off traversal.
//

mobj_t* soundtarget;

void P_RecursiveSound(sector_t* sec, int soundblocks) {
	int        i;
	line_t* check;
	sector_t* other;

	// wake up all monsters in this sector
	if (sec->validcount == validcount && sec->soundtraversed <= soundblocks + 1) {
		return;    // already flooded
	}

	sec->validcount = validcount;
	sec->soundtraversed = soundblocks + 1;

	P_SetTarget(&sec->soundtarget, soundtarget);

	for (i = 0; i < sec->linecount; i++) {
		check = sec->lines[i];
		if (!(check->flags & ML_TWOSIDED)) {
			continue;
		}

		P_LineOpening(check);

		if (openrange <= 0) {
			continue;    // closed door
		}

		if (sides[check->sidenum[0]].sector == sec) {
			other = sides[check->sidenum[1]].sector;
		}
		else {
			other = sides[check->sidenum[0]].sector;
		}

		if (check->flags & ML_SOUNDBLOCK) {
			if (!soundblocks) {
				P_RecursiveSound(other, 1);
			}
		}
		else {
			P_RecursiveSound(other, soundblocks);
		}
	}
}

//
// P_NoiseAlert
// If a monster yells at a player,
// it will alert other monsters to the player.
//

void P_NoiseAlert(mobj_t* target, mobj_t* emmiter) {
	soundtarget = target;
	validcount++;
	P_RecursiveSound(emmiter->subsector->sector, 0);
}

//
// P_CheckMeleeRange
//

boolean P_CheckMeleeRange(mobj_t* actor) {
	mobj_t* pl;
	fixed_t    dist;

	if (!(actor->flags & MF_SEETARGET)) {
		return false;
	}

	if (!actor->target) {
		return false;
	}

	pl = actor->target;
	dist = P_AproxDistance(pl->x - actor->x, pl->y - actor->y);

	if (dist >= MELEERANGE) {
		return false;
	}

	return true;
}

//
// P_CheckMissileRange
//

boolean P_CheckMissileRange(mobj_t* actor) {
	fixed_t    dist;

	if (!(actor->flags & MF_SEETARGET)) {
		return false;
	}

	if (actor->flags & MF_JUSTHIT) {
		/* the target just hit the enemy, so fight back! */
		actor->flags &= ~MF_JUSTHIT;
		return true;
	}

	if (actor->reactiontime) {
		return false;    // do not attack yet
	}

	dist = P_AproxDistance(actor->x - actor->target->x,
		actor->y - actor->target->y) - 64 * FRACUNIT;

	if (!actor->info->meleestate) {
		dist -= 128 * FRACUNIT;    // no melee attack, so fire more
	}

	dist >>= 16;

	if (actor->type == MT_VILE)
	{
		if (dist > 14 * 64)
			return false;	// too far away
	}

	if (actor->type == MT_RESURRECTOR2)
	{
		if (dist > 14 * 64)
			return false;	// too far away
	}

	if (actor->type == MT_SKULL) {
		dist >>= 1;
	}

	if (dist > 200) {
		dist = 200;
	}

	if (P_Random(pr_missrange) < dist) {
		return false;
	}

	return true;
}

//
// P_MissileAttack
//

enum {
	DP_STRAIGHT,
	DP_LEFT,
	DP_RIGHT
} dirproj_e;

static mobj_t* P_MissileAttack(mobj_t* actor, int direction) {
	angle_t angle = 0;
	fixed_t deltax = 0;
	fixed_t deltay = 0;
	fixed_t deltaz = 0;
	fixed_t offs = 0;
	int type = 0;
	boolean aim = false;
	mobj_t* mo;

	if (direction == DP_LEFT) {
		angle = actor->angle + ANG45;
	}
	else if (direction == DP_RIGHT) {
		angle = actor->angle - ANG45;
	}
	else {
		angle = actor->angle;
	}

	angle >>= ANGLETOFINESHIFT;

	switch (actor->type) {
	case MT_MANCUBUS:
		offs = 50;
		deltaz = 69;
		type = MT_PROJ_FATSO;
		aim = true;
		break;
	case MT_IMP1:
		offs = 0;
		deltaz = 64;
		type = MT_PROJ_IMP1;
		aim = true;
		break;
	case MT_IMP2:
		offs = 0;
		deltaz = 64;
		type = MT_PROJ_IMP2;
		aim = true;
		break;
	case MT_BABY:
		offs = 20;
		deltaz = 28;
		type = MT_PROJ_BABY;
		aim = false;
		break;
	case MT_CACODEMON:
		offs = 0;
		deltaz = 46;
		type = MT_PROJ_HEAD;
		aim = true;
		break;
	case MT_CYBORG:
	case MT_CYBORG_TITLE:
		offs = 45;
		deltaz = 88;
		type = MT_PROJ_ROCKET;
		aim = true;
		break;
	case MT_BRUISER1:
		offs = 0;
		deltaz = 48;
		type = MT_PROJ_BRUISER2;
		aim = true;
		break;
	case MT_BRUISER2:
		offs = 0;
		deltaz = 48;
		type = MT_PROJ_BRUISER1;
		aim = true;
		break;
	case MT_ANNIHILATOR:
		offs = 45;
		deltaz = 88;
		type = MT_PROJ_ROCKET;
		aim = true;
		break;
	case MT_HELLHOUND:
		offs = 20;
		deltaz = 46;
		type = MT_PROJ_HEAD;
		aim = true;
		break;
	case MT_DUKEOFHELL:
		offs = 50;
		deltaz = 69;
		type = MT_PROJ_DUKEOFHELL;
		aim = true;
		break;
	case MT_BRUISERDEMON:
		offs = 0;
		deltaz = 48;
		type = MT_PROJ_BRUISERDEMON1;
		aim = true;
		break;
	case MT_BELPHEGOR:
		offs = 0;
		deltaz = 48;
		type = MT_PROJ_BRUISER1;
		aim = true;
		break;
	case MT_HECTEBUS:
		offs = 50;
		deltaz = 69;
		type = MT_PROJ_HECTEBUS;
		aim = true;
		break;
	case MT_DARKIMP:
		offs = 0;
		deltaz = 64;
		type = MT_PROJ_DARKIMP;
		aim = true;
		break;
	case MT_CACOLANTERN:
		offs = 0;
		deltaz = 46;
		type = MT_PROJ_CACOLANTERN;
		aim = true;
		break;
	case MT_ABADDON:
		offs = 0;
		deltaz = 46;
		type = MT_PROJ_ABADDON;
		aim = true;
		break;
	case MT_NIGHTMARE_CACODEMON:
		offs = 0;
		deltaz = 46;
		type = MT_PROJ_NIGHTMARE_CACODEMON;
		aim = true;
		break;
	case MT_PAIN_ELEMENTAL_NIGHTMARE:
		offs = 32;
		deltaz = 32;
		type = MT_PROJ_PAIN_ELEMENTAL_NIGHTMARE;
		aim = true;
		break;
	case MT_NIGHTMARE_MANCUBUS:
		offs = 50;
		deltaz = 69;
		type = MT_PROJ_NIGHTMARE_MANCUBUS;
		aim = true;
		break;
	case MT_HELLCENTAUR:
		offs = 0;
		deltaz = 48;
		type = MT_PROJ_BRUISER1;
		aim = true;
		break;
	case MT_NIGHTCRAWLER:
		offs = 20;
		deltaz = 28;
		type = MT_PROJ_ROCKET;
		aim = false;
		break;
	case MT_HARDCORE_IMP:
		offs = 0;
		deltaz = 64;
		type = MT_PROJ_HARDCORE_IMP;
		aim = true;
		break;
	case MT_PlASMAZOMBIE:
		offs = 12;
		deltaz = 42;
		type = MT_PROJ_PLASMA;
		aim = true;
		break;
	case MT_BFGCOMMANDO:
		offs = 12;
		deltaz = 32;
		type = MT_PROJ_BFG;
		aim = true;
		break;
	case MT_BFGCYBERDEMON:
		offs = 45;
		deltaz = 88;
		type = MT_PROJ_BFG;
		aim = true;
		break;
	case MT_STALKER:
		offs = 0;
		deltaz = 32;
		type = MT_PROJ_STALKER1;
		aim = true;
		break;
	case MT_ARTHRONAILER:
		offs = 20;
		deltaz = 28;
		type = MT_PROJ_ARTHRONAILER;
		aim = false;
		break;
	}

	deltax = FixedMul(offs * FRACUNIT, finecosine[angle]);
	deltay = FixedMul(offs * FRACUNIT, finesine[angle]);

	mo = P_SpawnMissile(actor, actor->target, type, deltax, deltay, deltaz, aim);

	return mo;
}

//
// T_MobjExplode
//

void T_MobjExplode(mobjexp_t* mexp) {
	fixed_t x;
	fixed_t y;
	fixed_t z;
	mobj_t* exp;
	mobj_t* mobj = mexp->mobj;

	if (mexp->delay-- > 0) {
		return;
	}

	mexp->delay = mexp->delaymax;

	if (mobj->state != (state_t*)S_NULL) {
		x = (P_RandomShift(pr_mobjexplode, 14) + mobj->x);
		y = (P_RandomShift(pr_mobjexplode, 14) + mobj->y);
		z = (P_RandomShift(pr_mobjexplode, 14) + mobj->z);

		exp = P_SpawnMobj(x, y, z + (mobj->height << 1), MT_EXPLOSION2);

		if (!(mexp->lifetime & 1)) {
			S_StartSound(exp, sfx_explode);
		}
	}

	if (!mexp->lifetime--) {
		P_SetTarget(&mexp->mobj, NULL);
		P_RemoveThinker(&mexp->thinker);
	}
}

//
// P_Move
// Move in the current direction,
// returns false if the move is blocked.
//

fixed_t	xspeed[8] = { FRACUNIT,47000,0,-47000,-FRACUNIT,-47000,0,47000 };
fixed_t yspeed[8] = { 0,47000,FRACUNIT,47000,0,-47000,-FRACUNIT,-47000 };

boolean P_Move(mobj_t* actor) {
	fixed_t    tryx;
	fixed_t    tryy;
	line_t* ld;
	boolean   good;

	if (actor->movedir == DI_NODIR) {
		return false;
	}

	if ((actor->flags & MF_GRAVITY) != 0)
	{
		if (actor->floorz != actor->z)
		{
			return false;
		}
	}

	tryx = actor->x + actor->info->speed * xspeed[actor->movedir];
	tryy = actor->y + actor->info->speed * yspeed[actor->movedir];

	if (!P_TryMove(actor, tryx, tryy)) {
		// open any specials
		if (actor->flags & MF_FLOAT && floatok) {
			// must adjust height
			if (actor->z < tmfloorz) {
				actor->z += FLOATSPEED;
			}
			else {
				actor->z -= FLOATSPEED;
			}
			return true;
		}

		if (!numthingspec) {
			return false;
		}

		actor->movedir = DI_NODIR;
		good = false;

		while (numthingspec--) {
			ld = thingspec[numthingspec];
			// if the special is not a door that can be opened,
			// return false
			if (ld->special & MLU_USE) {
				if (P_UseSpecialLine(actor, ld, 0)) {
					good = true;
				}
			}
		}
		return good;
	}

	return true;
}

//
// P_TryWalk
// Attempts to move actor on
// in its current (ob->moveangle) direction.
// If blocked by either a wall or an actor
// returns FALSE
// If move is either clear or blocked only by a door,
// returns TRUE and sets...
// If a door is in the way,
// an OpenDoor call is made to start it opening.
//

boolean P_TryWalk(mobj_t* actor) {
	if (!P_Move(actor))
		return false;

	actor->movecount = P_Random(pr_trywalk) & 7;
	return true;
}

//
// P_NewChaseDir
//

void P_NewChaseDir(mobj_t* actor) {
	fixed_t        deltax;
	fixed_t        deltay;
	dirtype_t    d[3];
	int            tdir;
	dirtype_t    olddir;
	dirtype_t    turnaround;

	if (!actor->target) {
		I_Error("P_NewChaseDir: called with no target");
	}

	olddir = actor->movedir;
	turnaround = opposite[olddir];

	deltax = actor->target->x - actor->x;
	deltay = actor->target->y - actor->y;

	if (deltax > 10 * FRACUNIT) {
		d[1] = DI_EAST;
	}
	else if (deltax < -10 * FRACUNIT) {
		d[1] = DI_WEST;
	}
	else {
		d[1] = DI_NODIR;
	}

	if (deltay < -10 * FRACUNIT) {
		d[2] = DI_SOUTH;
	}
	else if (deltay > 10 * FRACUNIT) {
		d[2] = DI_NORTH;
	}
	else {
		d[2] = DI_NODIR;
	}

	// try direct route
	if (d[1] != DI_NODIR && d[2] != DI_NODIR) {
		actor->movedir = diags[((deltay < 0) << 1) + (deltax > 0)];
		if (actor->movedir != turnaround && P_TryWalk(actor)) {
			return;
		}
	}

	// try other directions
	if (P_Random(pr_newchase) > 200 || D_abs(deltay) > D_abs(deltax)) {
		tdir = d[1];
		d[1] = d[2];
		d[2] = tdir;
	}

	if (d[1] == turnaround) {
		d[1] = DI_NODIR;
	}
	if (d[2] == turnaround) {
		d[2] = DI_NODIR;
	}

	if (d[1] != DI_NODIR) {
		actor->movedir = d[1];
		if (P_TryWalk(actor)) {
			return;    // either moved forward or attacked
		}
	}

	if (d[2] != DI_NODIR) {
		actor->movedir = d[2];
		if (P_TryWalk(actor)) {
			return;
		}
	}

	// there is no direct path to the player,
	// so pick another direction.
	if (olddir != DI_NODIR) {
		actor->movedir = olddir;
		if (P_TryWalk(actor)) {
			return;
		}
	}

	// randomly determine direction of search
	if (P_Random(pr_newchasedir) & 1) {
		for (tdir = DI_EAST; tdir <= DI_SOUTHEAST; tdir++) {
			if (tdir != turnaround) {
				actor->movedir = tdir;
				if (P_TryWalk(actor)) {
					return;
				}
			}
		}
	}
	else {
		for (tdir = DI_SOUTHEAST; tdir != (DI_EAST - 1); tdir--) {
			if (tdir != turnaround) {
				actor->movedir = tdir;
				if (P_TryWalk(actor)) {
					return;
				}
			}
		}
	}

	if (turnaround != DI_NODIR) {
		actor->movedir = turnaround;
		if (P_TryWalk(actor)) {
			return;
		}
	}

	actor->movedir = DI_NODIR;    // can not move
}

//
// P_LookForPlayers
// If allaround is false, only look 180 degrees in front.
// Returns true if a player is targeted.
//

boolean P_LookForPlayers(mobj_t* actor, boolean allaround) {
	angle_t     an;
	fixed_t     dist;

	if (!actor->target || actor->target->health <= 0 ||
		!(actor->flags & MF_SEETARGET)) {
		if (actor->type > MT_PLAYERBOT3) { // for monsters
			int index = 0;

			// find other players
			if (netgame) {
				int i;
				int num = 0;
				int cur = 0;

				for (i = 0; i < MAXPLAYERS; i++) {
					if (playeringame[i]) {
						++num;
						if (actor->target == players[i].mo) {
							cur = i;
						}
					}
				}

				index = ((cur + 1) % num);

				// don't bother with dead targets
				if (players[index].mo->health <= 0) {
					return false;
				}
			}

			P_SetTarget(&actor->target, players[index].mo);
		}

		else {  // special case for player bots
			fixed_t dist2 = D_MAXINT;
			mobj_t* mobj;

			for (mobj = mobjhead.next; mobj != &mobjhead; mobj = mobj->next) {
				if (!(mobj->flags & MF_COUNTKILL) || mobj->type <= MT_PLAYERBOT3 ||
					mobj->health <= 0 || mobj == actor) {
					continue;
				}

				// find a killable target as close as possible
				dist = P_AproxDistance(mobj->x - actor->x, mobj->y - actor->y);
				if (!(dist < dist2)) {
					continue;
				}

				P_SetTarget(&actor->target, mobj);
				dist2 = dist;
			}
		}

		return false;
	}

	if (!actor->subsector->sector->soundtarget) {
		if (!allaround) {
			an = R_PointToAngle2(actor->x, actor->y,
				actor->target->x, actor->target->y) - actor->angle;

			if (an > ANG90 && an < ANG270) {
				dist = P_AproxDistance(actor->target->x - actor->x,
					actor->target->y - actor->y);

				// if real close, react anyway
				if (dist > MELEERANGE) {
					return false;    // behind back
				}
			}
		}
	}

	return true;
}

//
// ACTION ROUTINES
//

//
// A_Look
// Stay in state until a player is sighted.
//

void A_Look(mobj_t* actor) {
	mobj_t* targ;

	if (!P_LookForPlayers(actor, false)) {
		actor->threshold = 0;    // any shot will wake up
		targ = actor->subsector->sector->soundtarget;

		if (!targ) {
			return;
		}

		if (!(targ->flags & MF_SHOOTABLE)) {
			return;
		}

		if (actor->flags & MF_AMBUSH) {
			return;
		}

		P_SetTarget(&actor->target, targ);
	}

	// go into chase state
	if (actor->info->seesound) {
		int sound;

		switch (actor->info->seesound) {
		case sfx_possit1:
		case sfx_possit2:
		case sfx_possit3:
			sound = sfx_possit1 + (P_Random(pr_see) % 3);
			break;

		case sfx_impsit1:
		case sfx_impsit2:
			sound = sfx_impsit1 + (P_Random(pr_see) & 1);
			break;

		default:
			sound = actor->info->seesound;
			break;
		}

		if (actor->type == MT_RESURRECTOR || actor->type == MT_CYBORG || actor->type == MT_RESURRECTOR2) {
			// full volume
			S_StartSound(NULL, sound);
		}
		else {
			S_StartSound(actor, sound);
		}

		if (actor->type == MT_ANNIHILATOR) {
			// full volume
			S_StartSound(NULL, sound);
		}
		else {
			S_StartSound(actor, sound);
		}
	}

	P_SetMobjState(actor, actor->info->seestate);
}

//
// A_Chase
// Actor has a melee attack,
// so it tries to close as fast as possible
//
void A_Chase(mobj_t* actor) {
	int delta;

	if (actor->reactiontime) {
		actor->reactiontime--;
	}

	// modify target threshold
	if (actor->threshold) {
		if (!actor->target || actor->target->health <= 0) {
			actor->threshold = 0;
		}
		else {
			actor->threshold--;
		}
	}

	// turn towards movement direction if not there yet
	if (actor->movedir < 8) {
		actor->angle &= (7 << 29);
		delta = actor->angle - (actor->movedir << 29);

		if (delta > 0) {
			actor->angle -= ANG90 / 2;
		}
		else if (delta < 0) {
			actor->angle += ANG90 / 2;
		}
	}

	if (!actor->target || !(actor->target->flags & MF_SHOOTABLE)) {
		// look for a new target
		if (P_LookForPlayers(actor, true)) {
			return;    // got a new target
		}

		P_SetMobjState(actor, actor->info->spawnstate);
		return;
	}

	// do not attack twice in a row
	if (actor->flags & MF_JUSTATTACKED) {
		actor->flags &= ~MF_JUSTATTACKED;
		if (gameskill != sk_nightmare && !fastparm) {
			P_NewChaseDir(actor);
		}
		return;
	}

	// check for melee attack
	if (actor->info->meleestate && P_CheckMeleeRange(actor)) {
		if (actor->info->attacksound) {
			S_StartSound(actor, actor->info->attacksound);
		}

		P_SetMobjState(actor, actor->info->meleestate);
		return;
	}

	// check for missile attack
	if (actor->info->missilestate) {
		if (gameskill < sk_nightmare && !fastparm && actor->movecount) {
			goto nomissile;
		}

		if (!P_CheckMissileRange(actor)) {
			goto nomissile;
		}

		P_SetMobjState(actor, actor->info->missilestate);
		actor->flags |= MF_JUSTATTACKED;
		return;
	}

nomissile:
	// possibly choose another target
	if (netgame && !actor->threshold && !(actor->flags & MF_SEETARGET)) {
		if (P_LookForPlayers(actor, true)) {
			return;    // got a new target
		}
	}

	// chase towards player
	if (--actor->movecount < 0 || !P_Move(actor)) {
		P_NewChaseDir(actor);
	}

	// make active sound
	if (actor->info->activesound && P_Random(pr_see) < 3) {
		S_StartSound(actor, actor->info->activesound);
	}
}

//
// A_FaceTarget
//
void A_FaceTarget(mobj_t* actor) {
	

	if (!actor->target) {
		return;
	}

	actor->flags &= ~MF_AMBUSH;

	actor->angle = R_PointToAngle2(actor->x, actor->y, actor->target->x, actor->target->y);

	if (actor->target->flags & MF_SHADOW) {
		
		actor->angle += P_RandomShift(pr_facetarget, 21);
	}
}

//
// A_Tracer
//

#define    TRACEANGLE 0x10000000;

void A_Tracer(mobj_t* actor) {
	angle_t    exact;
	fixed_t    dist;
	fixed_t    slope;
	mobj_t* dest;
	mobj_t* th;

	th = P_SpawnMobj(actor->x - actor->momx,
		actor->y - actor->momy, actor->z, MT_SMOKE_RED);

	th->momz = FRACUNIT;
	th->tics -= P_Random(pr_tracer) & 3;
	if (th->tics < 1) {
		th->tics = 1;
	}

	if (actor->threshold-- < -100) {
		return;
	}

	// adjust direction
	dest = actor->tracer;

	if (!dest || dest->health <= 0) {
		return;
	}

	// change angle
	exact = R_PointToAngle2(actor->x, actor->y, dest->x, dest->y);

	if (exact != actor->angle) {
		if (exact - actor->angle > 0x80000000) {
			actor->angle -= TRACEANGLE;
			if (exact - actor->angle < 0x80000000) {
				actor->angle = exact;
			}
		}
		else {
			actor->angle += TRACEANGLE;
			if (exact - actor->angle > 0x80000000) {
				actor->angle = exact;
			}
		}
	}

	exact = actor->angle >> ANGLETOFINESHIFT;
	actor->momx = FixedMul(actor->info->speed, finecosine[exact]);
	actor->momy = FixedMul(actor->info->speed, finesine[exact]);

	// change slope
	dist = P_AproxDistance(dest->x - actor->x, dest->y - actor->y);
	dist = dist / actor->info->speed;

	if (dist < 1) {
		dist = 1;
	}

	slope = ((dest->height << 2) - dest->height);
	if (slope < 0) {
		slope = (((dest->height << 2) - dest->height) + 3);
	}

	slope = (dest->z + (slope >> 2) - actor->z) / dist;

	if (slope < actor->momz) {
		actor->momz -= (FRACUNIT >> 2);
	}
	else {
		actor->momz += (FRACUNIT >> 2);
	}
}

//
// A_OnDeathTrigger
//

void A_OnDeathTrigger(mobj_t* mo)
{
	mobj_t* mo2;

	if (!(mo->flags & MF_TRIGDEATH)) {
		return;
	}

	for (mo2 = mobjhead.next; mo2 != &mobjhead; mo2 = mo2->next)
	{
		if ((mo2->tid == mo->tid) && (mo2->health > 0))
		{
			return;
		}
	}

	P_QueueSpecial(mo);
}

//
// A_PosAttack
//

void A_PosAttack(mobj_t* actor) {
	int     angle;
	int     damage;
	int     hitdice;
	int     slope;
	
	if (!actor->target) {
		return;
	}

	S_StartSound(actor, sfx_pistol);
	A_FaceTarget(actor);

	angle = actor->angle;
	slope = P_AimLineAttack(actor, angle, 0, MISSILERANGE);

	
	angle += P_RandomShift(pr_posattack, 20);
	hitdice = (P_Random(pr_posattack) & 7);
	damage = ((hitdice << 2) - hitdice) + 3;
	P_LineAttack(actor, angle, MISSILERANGE, slope, damage);
}

//
// A_SPosAttack
//

void A_SPosAttack(mobj_t* actor) {
	int     i;
	int     angle;
	int     bangle;
	int     damage;
	int     slope;

	if (!actor->target) {
		return;
	}

	S_StartSound(actor, sfx_shotgun);
	A_FaceTarget(actor);
	bangle = actor->angle;
	slope = P_AimLineAttack(actor, bangle, 0, MISSILERANGE);

	for (i = 0; i < 3; i++) {
		angle = bangle + P_RandomShift(pr_sposattack, 20);
		damage = ((P_Random(pr_sposattack) % 5) * 3) + 3;
		P_LineAttack(actor, angle, MISSILERANGE, slope, damage);
	}
}

void A_CPosAttack(mobj_t* actor)
{
	int		angle;
	
	int		damage;
	int		slope;

	if (!actor->target)
		return;

	S_StartSound(actor, sfx_pistol);
	A_FaceTarget(actor);
	angle = actor->angle;
	slope = P_AimLineAttack(actor, angle, 0, MISSILERANGE);

	angle += P_RandomShift(pr_cposattack, 20);
	damage = ((P_Random(pr_cposattack) % 5) * 3) + 3;
	P_LineAttack(actor, angle, MISSILERANGE, slope, damage);
}

void A_CPosRefire(mobj_t* actor)
{
	// keep firing unless target got out of sight
	A_FaceTarget(actor);

	if (P_Random(pr_cposrefire) < 40)
		return;

	if (!actor->target
		|| actor->target->health <= 0
		|| !P_CheckSight(actor, actor->target))
	{
		P_SetMobjState(actor, actor->info->seestate);
	}
}

//
// A_PlayAttack
//

void A_PlayAttack(mobj_t* actor) {
	int    angle;
	int    damage;

	int    hitdice;
	int    slope;

	if (!actor->target) {
		return;
	}

	S_StartSound(actor, sfx_pistol);
	A_FaceTarget(actor);
	angle = actor->angle;

	slope = P_AimLineAttack(actor, angle, 0, MISSILERANGE);

	angle += P_RandomShift(pr_playattack, 20);
	hitdice = (P_Random(pr_playattack) % 5);
	damage = ((hitdice << 2) - hitdice) + 3;
	P_LineAttack(actor, angle, MISSILERANGE, slope, damage);
}

//
// A_BspiFaceTarget
//

void A_BspiFaceTarget(mobj_t* actor) {
	if (!actor->target) {
		return;
	}

	A_FaceTarget(actor);
	actor->reactiontime = 5;
}

//
// A_BspiAttack
//

void A_BspiAttack(mobj_t* actor) {
	if (!actor->target) {
		return;
	}

	A_FaceTarget(actor);
	P_MissileAttack(actor, DP_LEFT);
	P_MissileAttack(actor, DP_RIGHT);
}

//
// A_SpidRefire
//

void A_SpidRefire(mobj_t* actor) {
	A_FaceTarget(actor);

	if (P_Random(pr_spidrefire) < 10) {
		return;
	}

	if ((!actor->target || actor->target->health <= 0) || !(actor->flags & MF_SEETARGET)) {
		P_SetMobjState(actor, actor->info->seestate);
		actor->reactiontime = 5;
		return;
	}

	if (!actor->reactiontime--) {
		P_SetMobjState(actor, actor->info->missilestate);
		actor->reactiontime = 5;
	}
}

//
// A_TroopMelee
//

void A_TroopMelee(mobj_t* actor) {
	int    damage;
	int hitdice;

	if (!actor->target) {
		return;
	}

	if (P_CheckMeleeRange(actor)) {
		S_StartSound(actor, sfx_scratch);
		hitdice = (P_Random(pr_troopattack) & 7);
		damage = ((hitdice << 2) - hitdice) + 3;
		P_DamageMobj(actor->target, actor, actor, damage);
	}
}

//
// A_TroopAttack
//

void A_TroopAttack(mobj_t* actor) {
	if (!actor->target) {
		return;
	}

	A_FaceTarget(actor);

	// launch a missile
	P_MissileAttack(actor, DP_STRAIGHT);
}

//
// A_SargAttack
//

void A_SargAttack(mobj_t* actor) {
	int    damage;
	int    hitdice;

	if (!actor->target) {
		return;
	}

	A_FaceTarget(actor);
	if (P_CheckMeleeRange(actor)) {
		hitdice = (P_Random(pr_sargattack) & 7);
		damage = ((hitdice << 2) + 4);
		P_DamageMobj(actor->target, actor, actor, damage);
	}
}

//
// A_HeadAttack
//

void A_HeadAttack(mobj_t* actor) {
	int    damage;
	int hitdice;

	if (!actor->target) {
		return;
	}

	A_FaceTarget(actor);
	if (P_CheckMeleeRange(actor)) {
		S_StartSound(actor, sfx_scratch);
		hitdice = (P_Random(pr_headattack) & 7);
		damage = (hitdice << 3) + 8;
		P_DamageMobj(actor->target, actor, actor, damage);
		return;
	}

	// launch a missile
	P_MissileAttack(actor, DP_STRAIGHT);
}

//
// A_CyberAttack
//

void A_CyberAttack(mobj_t* actor) {
	if (!actor->target) {
		return;
	}

	A_FaceTarget(actor);
	P_MissileAttack(actor, DP_LEFT);
}

//
// A_CyberDeathEvent
//

void A_CyberDeathEvent(mobj_t* actor) {
	mobjexp_t* exp;

	exp = Z_Calloc(sizeof(*exp), PU_LEVSPEC, 0);
	P_AddThinker(&exp->thinker);

	exp->thinker.function.acp1 = (actionf_p1)T_MobjExplode;
	exp->delaymax = 4;
	exp->delay = 0;
	exp->lifetime = 12;
	P_SetTarget(&exp->mobj, actor);

	if (actor->info->deathsound) {
		S_StartSound(NULL, actor->info->deathsound);
	}
}

//
// A_BruisAttack
//

void A_BruisAttack(mobj_t* actor) {
	int    damage;
	int    hitdice;

	if (!actor->target)
	{
		return;
	}

	if (P_CheckMeleeRange(actor))
	{
		S_StartSound(actor, sfx_scratch);
		hitdice = (P_Random(pr_bruisattack) & 7);
		damage = ((hitdice * 11) + 11);
		P_DamageMobj(actor->target, actor, actor, damage);
		return;
	}

	// launch a missile
	P_MissileAttack(actor, DP_STRAIGHT);
}

//
// A_RectChase
//

void A_RectChase(mobj_t* actor) {
	if (!actor->target) {
		A_Chase(actor);
		return;
	}
	if (actor->target->health <= 0) {
		A_Chase(actor);
		return;
	}
	if (!(P_AproxDistance(actor->target->x - actor->x,
		actor->target->y - actor->y) < (600 * FRACUNIT))) {
		A_Chase(actor);
		return;
	}

	A_FaceTarget(actor);
	S_StartSound(actor, actor->info->attacksound);
	P_SetMobjState(actor, actor->info->meleestate);
}

//
// A_RectMissile
//

void A_RectMissile(mobj_t* actor) {
	mobj_t* mo;
	int count = 0;
	angle_t an = 0;
	fixed_t x = 0;
	fixed_t y = 0;

	if (!actor->target) {
		return;
	}

	A_FaceTarget(actor);
	for (mo = mobjhead.next; mo != &mobjhead; mo = mo->next) {
		// not a rect projectile
		if (mo->type != MT_PROJ_RECT) {
			continue;
		}

		count++;
	}

	if (!(count < 9)) {
		return;
	}

	// Arm 1

	an = (actor->angle - ANG90) >> ANGLETOFINESHIFT;
	x = FixedMul(68 * FRACUNIT, finecosine[an]);
	y = FixedMul(68 * FRACUNIT, finesine[an]);
	mo = P_SpawnMobj(actor->x + x, actor->y + y, actor->z + 68 * FRACUNIT, MT_PROJ_RECT);
	P_SetTarget(&mo->target, actor);
	P_SetTarget(&mo->tracer, actor->target);
	mo->threshold = 5;
	an = (actor->angle + ANG270);
	mo->angle = an;
	an >>= ANGLETOFINESHIFT;
	mo->momx = FixedMul(mo->info->speed, finecosine[an]);
	mo->momy = FixedMul(mo->info->speed, finesine[an]);

	// Arm2

	an = (actor->angle - ANG90) >> ANGLETOFINESHIFT;
	x = FixedMul(50 * FRACUNIT, finecosine[an]);
	y = FixedMul(50 * FRACUNIT, finesine[an]);
	mo = P_SpawnMobj(actor->x + x, actor->y + y, actor->z + 139 * FRACUNIT, MT_PROJ_RECT);
	P_SetTarget(&mo->target, actor);
	P_SetTarget(&mo->tracer, actor->target);
	mo->threshold = 1;
	an = (actor->angle + ANG270);
	mo->angle = an;
	an >>= ANGLETOFINESHIFT;
	mo->momx = FixedMul(mo->info->speed, finecosine[an]);
	mo->momy = FixedMul(mo->info->speed, finesine[an]);

	// Arm3

	an = (actor->angle + ANG90) >> ANGLETOFINESHIFT;
	x = FixedMul(68 * FRACUNIT, finecosine[an]);
	y = FixedMul(68 * FRACUNIT, finesine[an]);
	mo = P_SpawnMobj(actor->x + x, actor->y + y, actor->z + 68 * FRACUNIT, MT_PROJ_RECT);
	P_SetTarget(&mo->target, actor);
	P_SetTarget(&mo->tracer, actor->target);
	mo->threshold = 5;
	an = (actor->angle - ANG270);
	mo->angle = an;
	an >>= ANGLETOFINESHIFT;
	mo->momx = FixedMul(mo->info->speed, finecosine[an]);
	mo->momy = FixedMul(mo->info->speed, finesine[an]);

	// Arm4

	an = (actor->angle + ANG90) >> ANGLETOFINESHIFT;
	x = FixedMul(50 * FRACUNIT, finecosine[an]);
	y = FixedMul(50 * FRACUNIT, finesine[an]);
	mo = P_SpawnMobj(actor->x + x, actor->y + y, actor->z + 139 * FRACUNIT, MT_PROJ_RECT);
	P_SetTarget(&mo->target, actor);
	P_SetTarget(&mo->tracer, actor->target);
	mo->threshold = 1;
	an = (actor->angle - ANG270);
	mo->angle = an;
	an >>= ANGLETOFINESHIFT;
	mo->momx = FixedMul(mo->info->speed, finecosine[an]);
	mo->momy = FixedMul(mo->info->speed, finesine[an]);
}

//
// A_RectTracer
//

void A_RectTracer(mobj_t* actor) {
	if (actor->threshold < 0) {
		A_Tracer(actor);
	}
	else {
		actor->threshold--;
	}
}

//
// A_RectGroundFire
//

void A_RectGroundFire(mobj_t* actor) {
	mobj_t* mo;
	angle_t an;

	if (!actor->target) {
		return;
	}

	A_FaceTarget(actor);

	mo = P_SpawnMobj(actor->x, actor->y, actor->z, MT_PROJ_RECTFIRE);
	P_SetTarget(&mo->target, actor);
	an = actor->angle + R_PointToAngle2(actor->x, actor->y, mo->target->x, mo->target->y);

	mo->angle = an;
	mo->angle >>= ANGLETOFINESHIFT;
	mo->momx = FixedMul(mo->info->speed, finecosine[mo->angle]);
	mo->momy = FixedMul(mo->info->speed, finesine[mo->angle]);
	mo->angle = an;

	mo = P_SpawnMobj(actor->x, actor->y, actor->z, MT_PROJ_RECTFIRE);
	P_SetTarget(&mo->target, actor);
	mo->angle = an - ANG45;
	mo->angle >>= ANGLETOFINESHIFT;
	mo->momx = FixedMul(mo->info->speed, finecosine[mo->angle]);
	mo->momy = FixedMul(mo->info->speed, finesine[mo->angle]);
	mo->angle = an - ANG45;

	mo = P_SpawnMobj(actor->x, actor->y, actor->z, MT_PROJ_RECTFIRE);
	P_SetTarget(&mo->target, actor);
	mo->angle = an + ANG45;
	mo->angle >>= ANGLETOFINESHIFT;
	mo->momx = FixedMul(mo->info->speed, finecosine[mo->angle]);
	mo->momy = FixedMul(mo->info->speed, finesine[mo->angle]);
	mo->angle = an + ANG45;

	S_StartSound(mo, mo->info->seesound);
}

//
// A_MoveGroundFire
//

void A_MoveGroundFire(mobj_t* fire) {
	mobj_t* mo;

	if (fire->z > fire->floorz && fire->momz > 0) {
		fire->z = fire->floorz;
	}

	mo = P_SpawnMobj(fire->x, fire->y, fire->floorz, MT_PROP_FIRE);
	P_FadeMobj(mo, -8, 0, 0);
}

//
// A_RectDeathEvent
//

void A_RectDeathEvent(mobj_t* actor) {
	mobjexp_t* exp;

	exp = Z_Calloc(sizeof(*exp), PU_LEVSPEC, 0);
	P_AddThinker(&exp->thinker);

	exp->thinker.function.acp1 = (actionf_p1)T_MobjExplode;
	exp->delaymax = 3;
	exp->delay = 0;
	exp->lifetime = 32;
	P_SetTarget(&exp->mobj, actor);

	if (actor->info->deathsound) {
		S_StartSound(NULL, actor->info->deathsound);
	}
}

// A_FatRaise
//
// Mancubus attack,
// firing three missiles (bruisers)
// in three different directions?
// Doesn't look like it.
//

void A_FatRaise(mobj_t* actor) {
	if (!actor->target) {
		return;
	}

	A_FaceTarget(actor);
	S_StartSound(actor, sfx_fattatk);
}

//
// A_FatAttack1
//

#define    FATSPREAD    0x10000000

void A_FatAttack1(mobj_t* actor) {
	mobj_t* mo;
	angle_t an;

	if (!actor->target) {
		return;
	}

	A_FaceTarget(actor);
	P_MissileAttack(actor, DP_RIGHT);
	mo = P_MissileAttack(actor, DP_LEFT);

	mo->angle += FATSPREAD;
	an = mo->angle >> ANGLETOFINESHIFT;

	mo->momx = FixedMul(mo->info->speed, finecosine[an]);
	mo->momy = FixedMul(mo->info->speed, finesine[an]);
}

//
// A_FatAttack2
//

void A_FatAttack2(mobj_t* actor) {
	mobj_t* mo;
	angle_t an;

	if (!actor->target) {
		return;
	}

	A_FaceTarget(actor);
	P_MissileAttack(actor, DP_LEFT);
	mo = P_MissileAttack(actor, DP_RIGHT);

	mo->angle -= FATSPREAD;
	an = mo->angle >> ANGLETOFINESHIFT;

	mo->momx = FixedMul(mo->info->speed, finecosine[an]);
	mo->momy = FixedMul(mo->info->speed, finesine[an]);
}

//
// A_FatAttack3
//

void A_FatAttack3(mobj_t* actor) {
	mobj_t* mo;
	angle_t an;

	if (!actor->target) {
		return;
	}

	A_FaceTarget(actor);
	mo = P_MissileAttack(actor, DP_RIGHT);
	mo->angle -= 0x4000000;
	an = mo->angle >> ANGLETOFINESHIFT;

	mo->momx = FixedMul(mo->info->speed, finecosine[an]);
	mo->momy = FixedMul(mo->info->speed, finesine[an]);

	mo = P_MissileAttack(actor, DP_LEFT);
	mo->angle += 0x4000000;
	an = mo->angle >> ANGLETOFINESHIFT;

	mo->momx = FixedMul(mo->info->speed, finecosine[an]);
	mo->momy = FixedMul(mo->info->speed, finesine[an]);
}

//
// A_SkullAttack
// Fly at the player like a missile.
//
#define    SKULLSPEED      (40*FRACUNIT)

void A_SkullAttack(mobj_t* actor) {
	mobj_t* dest;
	angle_t        an;
	int            dist;

	if (!actor->target) {
		return;
	}

	dest = actor->target;
	actor->flags |= MF_SKULLFLY;

	S_StartSound(actor, actor->info->attacksound);
	A_FaceTarget(actor);

	an = actor->angle >> ANGLETOFINESHIFT;
	actor->momx = FixedMul(SKULLSPEED, finecosine[an]);
	actor->momy = FixedMul(SKULLSPEED, finesine[an]);
	dist = P_AproxDistance(dest->x - actor->x, dest->y - actor->y);
	dist = dist / SKULLSPEED;

	if (dist < 1) {
		dist = 1;
	}

	actor->momz = (dest->z + (dest->height >> 1) - actor->z) / dist;
}

//
// A_SkullSetAlpha
//

void A_SkullSetAlpha(mobj_t* actor) {
	actor->alpha >>= 2;
}

//
// PIT_PainCheckLine
//

static boolean PIT_PainCheckLine(intercept_t* in) {
	if (!in->d.line->backsector) {
		return false;
	}

	return true;
}

//
// A_PainShootSkull
// Spawn a lost soul and launch it at the target
//

void A_PainShootSkull(mobj_t* actor, angle_t angle) {
	fixed_t     x;
	fixed_t     y;
	fixed_t     z;
	mobj_t* newmobj;
	angle_t     an;
	int         prestep;
	int         count;

	// count total number of skull currently on the level
	count = 0;

	for (newmobj = mobjhead.next; newmobj != &mobjhead; newmobj = newmobj->next) {
		if (newmobj->type == MT_SKULL) {
			count++;
		}
	}

	// if there are all ready 17 skulls on the level, don't spit another one
	if (count >= 17) {
		return;
	}

	an = angle >> ANGLETOFINESHIFT;

	prestep = 4 * FRACUNIT + 3 * (actor->info->radius + mobjinfo[MT_SKULL].radius) / 2;

	x = actor->x + FixedMul(prestep, finecosine[an]);
	y = actor->y + FixedMul(prestep, finesine[an]);
	z = actor->z + 16 * FRACUNIT;

	newmobj = P_SpawnMobj(x, y, z, MT_SKULL);

	// Check for movements

	if ((!P_TryMove(newmobj, newmobj->x, newmobj->y)) ||
		(!P_PathTraverse(actor->x, actor->y, newmobj->x, newmobj->y, PT_ADDLINES, PIT_PainCheckLine))) {
		// kill it immediately

		P_DamageMobj(newmobj, actor, actor, 10000);
		P_RadiusAttack(newmobj, newmobj, 128);
		return;
	}

	P_SetTarget(&newmobj->target, actor->target);
	P_SetMobjState(newmobj, newmobj->info->missilestate);
	A_SkullAttack(newmobj);
}

//
// A_PainAttack
// Spawn a lost soul and launch it at the target
//

void A_PainAttack(mobj_t* actor) {
	if (!actor->target) {
		return;
	}

	A_FaceTarget(actor);
	A_PainShootSkull(actor, actor->angle + 0x15550000);
	A_PainShootSkull(actor, actor->angle - 0x15550000);
}

//
// A_PainDie
//

void A_PainDie(mobj_t* actor) {
	A_Fall(actor);
	A_PainShootSkull(actor, actor->angle + ANG90);
	A_PainShootSkull(actor, actor->angle + ANG180);
	A_PainShootSkull(actor, actor->angle + ANG270);

	A_OnDeathTrigger(actor);
}

//
// A_PainDeathEvent
//

void A_PainDeathEvent(mobj_t* actor) {
	actor->alpha -= 0x3F;
}

//
// A_FadeOut
//

void A_FadeOut(mobj_t* actor) {
	P_FadeMobj(actor, -8, 0x30, 0);
}

//
// A_FadeIn
//

void A_FadeIn(mobj_t* actor) {
	P_FadeMobj(actor, 8, 0xff, 0);
}

//
// A_Scream
//

void A_Scream(mobj_t* actor) {
	int sound;

	switch (actor->info->deathsound) {
	case 0:
		return;

	case sfx_posdie1:
	case sfx_posdie2:
	case sfx_posdie3:
		sound = sfx_posdie1 + (P_Random(pr_scream) % 3);
		break;

	case sfx_impdth1:
	case sfx_impdth2:
		sound = sfx_impdth1 + (P_Random(pr_scream) & 1);
		break;

	default:
		sound = actor->info->deathsound;
		break;
	}

	S_StartSound(actor, sound);
}

//
// A_XScream
//

void A_XScream(mobj_t* actor) {
	S_StartSound(actor, sfx_slop);
}

//
// A_Pain
//

void A_Pain(mobj_t* actor) {
	if (actor->info->painsound) {
		if (actor->type == MT_RESURRECTOR) {
			S_StartSound(NULL, actor->info->painsound);
		}
		else {
			S_StartSound(actor, actor->info->painsound);
		}
	}
}

//
// A_Fall
//

void A_Fall(mobj_t* actor) {
	// actor is on ground, it can be walked over
	actor->flags &= ~MF_SOLID;
	actor->blockflag |= BF_MIDPOINTONLY;
}

//
// A_Explode
//

void A_Explode(mobj_t* thingy) {
	P_RadiusAttack(thingy, thingy->target, 128);
}

//
// A_BarrelExplode
//

void A_BarrelExplode(mobj_t* actor) {
	mobj_t* exp;

	S_StartSound(actor, sfx_explode);
	exp = P_SpawnMobj(actor->x + FRACUNIT, actor->y + FRACUNIT, actor->z + (actor->height << 1), MT_EXPLOSION1);
	A_Explode(actor);

	A_OnDeathTrigger(actor);
}

//
// A_Hoof
//

void A_Hoof(mobj_t* mo) {
	S_StartSound(mo, sfx_cybhoof);
	A_Chase(mo);
}

//
// A_Metal
//

void A_Metal(mobj_t* mo) {
	S_StartSound(mo, sfx_metal);
	A_Chase(mo);
}

//
// A_BabyMetal
//

void A_BabyMetal(mobj_t* mo) {
	S_StartSound(mo, sfx_bspistomp);
	A_Chase(mo);
}

//
// A_SpiderMastermindMetal
//

void A_SpiderMastermindMetal(mobj_t* mo) {
	S_StartSound(mo, sfx_spistomp);
	A_Chase(mo);
}

//
// A_PlayerScream
//

void A_PlayerScream(mobj_t* mo) {
	int    sound = sfx_plrdie;
	S_StartSound(mo, sound);
}

//
// A_SpawnSmoke
//

void A_SpawnSmoke(mobj_t* mobj) {
	mobj_t* smoke = P_SpawnMobj(mobj->x, mobj->y, mobj->z, MT_SMOKE_GRAY);
	smoke->momz = FRACUNIT;

	if (mobj->type == MT_PROJ_GRENADE)
	{
		mobj->reactiontime -= 8;
		if (mobj->reactiontime <= 0)
		{
			mobj->momx = mobj->momy = mobj->momz = 0;

			P_SetMobjState(mobj, mobj->info->deathstate);
			S_StartSound(mobj, mobj->info->deathsound, mobj);
		}
	}
}

//
// A_MissileSetAlpha
//

void A_MissileSetAlpha(mobj_t* actor) {
	actor->alpha >>= 1;
}

//
// A_FadeAlpha
//

void A_FadeAlpha(mobj_t* mobj) {
	int fade = 0;

	fade = ((mobj->alpha << 2) - mobj->alpha);
	if (!(fade >= 0)) {
		fade = fade + 3;
	}

	mobj->alpha = (fade >> 2);
}

//
// A_TargetCamera
//

void A_TargetCamera(mobj_t* actor) {
	mobj_t* mo;

	actor->threshold = D_MAXINT;

	for (mo = mobjhead.next; mo != &mobjhead; mo = mo->next) {
		if (actor->tid + 1 == mo->tid) {
			P_SetTarget(&actor->target, mo);
			P_SetMobjState(actor, actor->info->missilestate);
			return;
		}
	}
}

//
// A_SkelMissile
//

void A_SkelMissile(mobj_t* actor, int direction)
{
	mobj_t* mo;
	angle_t angle;

	if (direction == DP_LEFT) {
		angle = actor->angle + ANG45;
	}
	else if (direction == DP_RIGHT) {
		angle = actor->angle - ANG45;
	}
	else {
		angle = actor->angle;
	}
	angle >>= ANGLETOFINESHIFT;

	mo = P_SpawnMissile(actor,
		actor->target,
		MT_PROJ_UNDEAD,
		FixedMul(26 * FRACUNIT, finecosine[angle]),
		FixedMul(26 * FRACUNIT, finesine[angle]),
		104,
		true);
	mo->x += mo->momx;
	mo->y += mo->momy;
	mo->tracer = actor->target;
}

//
// A_SkelAttack
//

void A_SkelAttack(mobj_t* actor)
{
	if (!actor->target)
		return;
	A_FaceTarget(actor);
	A_SkelMissile(actor, DP_LEFT);
	A_SkelMissile(actor, DP_RIGHT);
}

//
// A_SkelWhoosh
//

void A_SkelWhoosh(mobj_t* actor)
{
	if (!actor->target)
		return;
	A_FaceTarget(actor);
	S_StartSound(actor, sfx_dart);
}

//
// A_SkelFist
//

void A_SkelFist(mobj_t* actor)
{
	int	damage;

	if (!actor->target)
		return;

	A_FaceTarget(actor);

	if (P_CheckMeleeRange(actor))
	{
		damage = ((P_Random(pr_skelfist) % 10) + 1) * 6;
		S_StartSound(actor, sfx_dartshoot);
		P_DamageMobj(actor->target, actor, actor, damage);
	}
}

//
// A_SpidAttack
//

void A_SpidAttack(mobj_t* actor)
{
	int i;
	int angle;
	int bangle;
	int damage;
	int slope;

	if (!actor->target)
		return;

	S_StartSound(actor, sfx_pistol);
	A_FaceTarget(actor);
	bangle = actor->angle;
	slope = P_AimLineAttack(actor, bangle, 0, MISSILERANGE);

	for (i = 0; i < 3; i++)
	{
		angle = bangle + P_RandomShift(pr_spidattack, 20);
		damage = ((P_Random(pr_spidattack) & 5) * 3) + 3;
		P_LineAttack(actor, angle, MISSILERANGE, slope, damage);
	}
}

//
// A_AnnihilatorAttack
//

void A_AnnihilatorAttack(mobj_t* actor) {
	if (!actor->target) {
		return;
	}

	A_FaceTarget(actor);
	P_MissileAttack(actor, DP_LEFT);
	P_MissileAttack(actor, DP_RIGHT);
}

//
// A_AnnihilatorHoof
//

void A_AnnihilatorHoof(mobj_t* mo) {
	S_StartSound(mo, sfx_annhoof);
	A_Chase(mo);
}

//
// A_AnnihilatorAttack1
//

#define    FATSPREAD    0x10000000

void A_AnnihilatorAttack1(mobj_t* actor) {
	mobj_t* mo;
	angle_t an;

	if (!actor->target) {
		return;
	}

	A_FaceTarget(actor);
	P_MissileAttack(actor, DP_RIGHT);
	mo = P_MissileAttack(actor, DP_LEFT);

	mo->angle += FATSPREAD;
	an = mo->angle >> ANGLETOFINESHIFT;

	mo->momx = FixedMul(mo->info->speed, finecosine[an]);
	mo->momy = FixedMul(mo->info->speed, finesine[an]);
}

//
// A_AnnihilatorAttack2
//

void A_AnnihilatorAttack2(mobj_t* actor) {
	mobj_t* mo;
	angle_t an;

	if (!actor->target) {
		return;
	}

	A_FaceTarget(actor);
	P_MissileAttack(actor, DP_LEFT);
	mo = P_MissileAttack(actor, DP_RIGHT);

	mo->angle -= FATSPREAD;
	an = mo->angle >> ANGLETOFINESHIFT;

	mo->momx = FixedMul(mo->info->speed, finecosine[an]);
	mo->momy = FixedMul(mo->info->speed, finesine[an]);
}

//
// A_AnnihilatorAttack3
//

void A_AnnihilatorAttack3(mobj_t* actor) {
	mobj_t* mo;
	angle_t an;

	if (!actor->target) {
		return;
	}

	A_FaceTarget(actor);
	mo = P_MissileAttack(actor, DP_RIGHT);
	mo->angle -= 0x4000000;
	an = mo->angle >> ANGLETOFINESHIFT;

	mo->momx = FixedMul(mo->info->speed, finecosine[an]);
	mo->momy = FixedMul(mo->info->speed, finesine[an]);

	mo = P_MissileAttack(actor, DP_LEFT);
	mo->angle += 0x4000000;
	an = mo->angle >> ANGLETOFINESHIFT;

	mo->momx = FixedMul(mo->info->speed, finecosine[an]);
	mo->momy = FixedMul(mo->info->speed, finesine[an]);
}

//
// A_HellhoundAttack
//

void A_HellhoundAttack(mobj_t* actor) {
	int    damage;
	int hitdice;

	if (!actor->target) {
		return;
	}

	A_FaceTarget(actor);
	if (P_CheckMeleeRange(actor)) {
		S_StartSound(actor, sfx_scratch);
		hitdice = (P_Random(pr_hellhoundattack) & 7);
		damage = (hitdice << 3) + 8;
		P_DamageMobj(actor->target, actor, actor, damage);
		return;
	}

	// launch a missile
	P_MissileAttack(actor, DP_LEFT);
	P_MissileAttack(actor, DP_RIGHT);
}

//
// A_DukeOfHellRaise
//

void A_DukeOfHellRaise(mobj_t* actor) {
	if (!actor->target) {
		return;
	}

	A_FaceTarget(actor);
	S_StartSound(actor, sfx_dkhlatk);
}

//
// A_DukeOfHellAttack1
//

#define    FATSPREAD    0x10000000

void A_DukeOfHellAttack1(mobj_t* actor) {
	mobj_t* mo;
	angle_t an;

	if (!actor->target) {
		return;
	}

	A_FaceTarget(actor);
	P_MissileAttack(actor, DP_RIGHT);
	mo = P_MissileAttack(actor, DP_LEFT);

	mo->angle += FATSPREAD;
	an = mo->angle >> ANGLETOFINESHIFT;

	mo->momx = FixedMul(mo->info->speed, finecosine[an]);
	mo->momy = FixedMul(mo->info->speed, finesine[an]);
}

//
// A_DukeOfHellAttack2
//

void A_DukeOfHellAttack2(mobj_t* actor) {
	mobj_t* mo;
	angle_t an;

	if (!actor->target) {
		return;
	}

	A_FaceTarget(actor);
	P_MissileAttack(actor, DP_LEFT);
	mo = P_MissileAttack(actor, DP_RIGHT);

	mo->angle -= FATSPREAD;
	an = mo->angle >> ANGLETOFINESHIFT;

	mo->momx = FixedMul(mo->info->speed, finecosine[an]);
	mo->momy = FixedMul(mo->info->speed, finesine[an]);
}

//
// A_DukeOfHellAttack3
//

void A_DukeOfHellAttack3(mobj_t* actor) {
	mobj_t* mo;
	angle_t an;

	if (!actor->target) {
		return;
	}

	A_FaceTarget(actor);
	mo = P_MissileAttack(actor, DP_RIGHT);
	mo->angle -= 0x4000000;
	an = mo->angle >> ANGLETOFINESHIFT;

	mo->momx = FixedMul(mo->info->speed, finecosine[an]);
	mo->momy = FixedMul(mo->info->speed, finesine[an]);

	mo = P_MissileAttack(actor, DP_LEFT);
	mo->angle += 0x4000000;
	an = mo->angle >> ANGLETOFINESHIFT;

	mo->momx = FixedMul(mo->info->speed, finecosine[an]);
	mo->momy = FixedMul(mo->info->speed, finesine[an]);
}

//
// A_BruiserDemonRandomAttack
// 

void A_BruiserDemonRandomAttack(mobj_t* actor) {


	if (P_Random(pr_bruiserdemondecide) < 220)
	{
		P_SetMobjState(actor, S_BR64_ATK1_1);
	}
	else
	{
		P_SetMobjState(actor, S_BR64_ATK1_2);
	}


}

//
// A_BruiserDemonFire
//

void A_BruiserDemonFire(mobj_t* actor) {
	mobj_t* mo;
	angle_t an;

	if (!actor->target) {
		return;
	}

	A_FaceTarget(actor);

	mo = P_SpawnMobj(actor->x, actor->y, actor->z, MT_PROJ_BRUISERDEMON2);
	P_SetTarget(&mo->target, actor);
	an = actor->angle + R_PointToAngle2(actor->x, actor->y, mo->target->x, mo->target->y);

	mo->angle = an;
	mo->angle >>= ANGLETOFINESHIFT;
	mo->momx = FixedMul(mo->info->speed, finecosine[mo->angle]);
	mo->momy = FixedMul(mo->info->speed, finesine[mo->angle]);
	mo->angle = an;

	mo = P_SpawnMobj(actor->x, actor->y, actor->z, MT_PROJ_BRUISERDEMON2);
	P_SetTarget(&mo->target, actor);
	mo->angle = an - ANG45;
	mo->angle >>= ANGLETOFINESHIFT;
	mo->momx = FixedMul(mo->info->speed, finecosine[mo->angle]);
	mo->momy = FixedMul(mo->info->speed, finesine[mo->angle]);
	mo->angle = an - ANG45;

	mo = P_SpawnMobj(actor->x, actor->y, actor->z, MT_PROJ_BRUISERDEMON2);
	P_SetTarget(&mo->target, actor);
	mo->angle = an + ANG45;
	mo->angle >>= ANGLETOFINESHIFT;
	mo->momx = FixedMul(mo->info->speed, finecosine[mo->angle]);
	mo->momy = FixedMul(mo->info->speed, finesine[mo->angle]);
	mo->angle = an + ANG45;

	S_StartSound(mo, mo->info->seesound);
}

//
// A_BruiserDemonExplodeFire
//

void A_BruiserDemonExplodeFire(mobj_t* thingy) {
	P_RadiusAttack(thingy, thingy->target, 128);
	S_StartSound(thingy, sfx_explode);
}

//
// PIT_VileCheck
// Detect a corpse that could be raised.
//
mobj_t* corpsehit;
mobj_t* vileobj;
fixed_t		viletryx;
fixed_t		viletryy;

boolean PIT_VileCheck(mobj_t* thing)
{
	int		maxdist;
	boolean	check;

	if (!(thing->flags & MF_CORPSE))
		return true;	// not a monster

	if (thing->tics != -1)
		return true;	// not lying still yet

	if (thing->info->raisestate == S_NULL)
		return true;	// monster doesn't have a raise state

	maxdist = thing->info->radius + mobjinfo[MT_VILE].radius;
	maxdist = thing->info->radius + mobjinfo[MT_RESURRECTOR2].radius;
	maxdist = thing->info->radius + mobjinfo[MT_NIGHTMARE_LOSTSOUL].radius;

	if (abs(thing->x - viletryx) > maxdist
		|| abs(thing->y - viletryy) > maxdist)
		return true;		// not actually touching

	corpsehit = thing;
	corpsehit->momx = corpsehit->momy = 0;
	corpsehit->height <<= 2;
	check = P_CheckPosition(corpsehit, corpsehit->x, corpsehit->y);
	corpsehit->height >>= 2;

	if (!check)
		return true;		// doesn't fit here

	return false;		// got one, so stop checking
}

//
// A_VileChase
// Check for ressurecting a body
//
void A_VileChase(mobj_t* actor)
{
	int			xl;
	int			xh;
	int			yl;
	int			yh;

	int			bx;
	int			by;

	mobjinfo_t* info;
	mobj_t* temp;

	if (actor->movedir != DI_NODIR)
	{
		// check for corpses to raise
		viletryx =
			actor->x + actor->info->speed * xspeed[actor->movedir];
		viletryy =
			actor->y + actor->info->speed * yspeed[actor->movedir];

		xl = (viletryx - bmaporgx - MAXRADIUS * 2) >> MAPBLOCKSHIFT;
		xh = (viletryx - bmaporgx + MAXRADIUS * 2) >> MAPBLOCKSHIFT;
		yl = (viletryy - bmaporgy - MAXRADIUS * 2) >> MAPBLOCKSHIFT;
		yh = (viletryy - bmaporgy + MAXRADIUS * 2) >> MAPBLOCKSHIFT;

		vileobj = actor;
		for (bx = xl; bx <= xh; bx++)
		{
			for (by = yl; by <= yh; by++)
			{
				// Call PIT_VileCheck to check
				// whether object is a corpse
				// that canbe raised.
				if (!P_BlockThingsIterator(bx, by, PIT_VileCheck))
				{
					// got one!
					temp = actor->target;
					actor->target = corpsehit;
					A_FaceTarget(actor);
					actor->target = temp;

					P_SetMobjState(actor, S_VILE_HEAL1);
					S_StartSound(corpsehit, sfx_slop);
					info = corpsehit->info;

					P_SetMobjState(corpsehit, info->raisestate);
					corpsehit->height <<= 2;
					corpsehit->flags = info->flags;
					corpsehit->health = info->spawnhealth;
					corpsehit->target = NULL;

					return;
				}
			}
		}
	}

	// Return to normal attack.
	A_Chase(actor);
}

//
// A_VileStart
//
void A_VileStart(mobj_t* actor)
{
	S_StartSound(actor, sfx_vilatk);
}

//
// A_Fire
// Keep fire in front of player unless out of sight
//
void A_Fire(mobj_t* actor);

void A_StartFire(mobj_t* actor)
{
	S_StartSound(actor, sfx_flamst);
	A_Fire(actor);
}

void A_FireCrackle(mobj_t* actor)
{
	S_StartSound(actor, sfx_flame);
	A_Fire(actor);
}

void A_Fire(mobj_t* actor)
{
	mobj_t* dest;
	unsigned	an;

	dest = actor->tracer;
	if (!dest)
		return;

	// don't move it if the vile lost sight
	if (!P_CheckSight(actor->target, dest))
		return;

	an = dest->angle >> ANGLETOFINESHIFT;

	P_UnsetThingPosition(actor);
	actor->x = dest->x + FixedMul(24 * FRACUNIT, finecosine[an]);
	actor->y = dest->y + FixedMul(24 * FRACUNIT, finesine[an]);
	actor->z = dest->z;
	P_SetThingPosition(actor);
}

//
// A_VileTarget
// Spawn the hellfire
//
void A_VileTarget(mobj_t* actor)
{
	mobj_t* fog;

	if (!actor->target)
		return;

	A_FaceTarget(actor);

	fog = P_SpawnMobj(actor->target->x,
		actor->target->x,
		actor->target->z, MT_FIRE);

	actor->tracer = fog;
	fog->target = actor;
	fog->tracer = actor->target;
	A_Fire(fog);
}

//
// A_VileAttack
//
void A_VileAttack(mobj_t* actor)
{
	mobj_t* fire;
	int		an;

	if (!actor->target)
		return;

	A_FaceTarget(actor);

	if (!P_CheckSight(actor, actor->target))
		return;

	S_StartSound(actor, sfx_explode);
	P_DamageMobj(actor->target, actor, actor, 20);
	actor->target->momz = 1000 * FRACUNIT / actor->target->info->mass;

	an = actor->angle >> ANGLETOFINESHIFT;

	fire = actor->tracer;

	if (!fire)
		return;

	// move the fire between the vile and the player
	fire->x = actor->target->x - FixedMul(24 * FRACUNIT, finecosine[an]);
	fire->y = actor->target->y - FixedMul(24 * FRACUNIT, finesine[an]);
	P_RadiusAttack(fire, actor, 70);
}

//
// A_SpidDeathEvent
//
void A_SpidDeathEvent(mobj_t* actor)
{
	mobjexp_t* exp;

	exp = Z_Calloc(sizeof(*exp), PU_LEVSPEC, 0);
	P_AddThinker(&exp->thinker);

	exp->thinker.function.acp1 = (actionf_p1)T_MobjExplode;
	exp->delaymax = 2;
	exp->delay = 0;
	exp->lifetime = 14;
	P_SetTarget(&exp->mobj, actor);

	if (actor->info->deathsound) {
		S_StartSound(NULL, actor->info->deathsound);
	}
}

//
// A_MeleeZombieAttack
//

void A_MeleeZombieAttack(mobj_t* actor) {
	int    damage;
	int hitdice;

	if (!actor->target) {
		return;
	}

	if (P_CheckMeleeRange(actor)) {
		S_StartSound(actor, sfx_punch);
		hitdice = (P_Random(pr_meleezombieattack) & 7);
		damage = ((hitdice << 2) - hitdice) + 3;
		P_DamageMobj(actor->target, actor, actor, damage);
	}
}

//
// A_SSGPosAttack
//

void A_SSGPosAttack(mobj_t* actor) {
	int     i;
	int     angle;
	int     bangle;
	int     damage;
	int     slope;

	if (!actor->target) {
		return;
	}

	S_StartSound(actor, sfx_sht2fire);
	A_FaceTarget(actor);
	bangle = actor->angle;
	slope = P_AimLineAttack(actor, bangle, 0, MISSILERANGE);

	for (i = 0; i < 7; i++) {
		damage = 5 * (P_Random(pr_ssgposattack) % 3 + 1);
		angle = bangle + ((P_Random(pr_ssgposattack) - P_Random(pr_ssgposattack)) << 20);
		angle += P_RandomShift(pr_shotgun, 19);
		P_LineAttack(actor, angle, MISSILERANGE, slope, damage);
	}
}

//
// A_BrainAwake
//

mobj_t* braintargets[32];
int		numbraintargets;
int		braintargeton;

void A_BrainAwake(mobj_t* mo)
{
	thinker_t* thinker;
	mobj_t* m;

	// find all the target spots
	numbraintargets = 0;
	braintargeton = 0;

	thinker = thinkercap.next;
	for (thinker = thinkercap.next;
		thinker != &thinkercap;
		thinker = thinker->next)
	{
		if (thinker->function.acp1 != (actionf_p1)P_MobjThinker)
			continue;	// not a mobj

		m = (mobj_t*)thinker;

		if (m->type == MT_BOSSTARGET)
		{
			braintargets[numbraintargets] = m;
			numbraintargets++;
		}
	}

	S_StartSound(NULL, sfx_bossit);
}

//
// A_BrainPain
//

void A_BrainPain(mobj_t* mo)
{
	S_StartSound(NULL, sfx_bospn);
}

//
// A_BrainScream
//

void A_BrainScream(mobj_t* mo)
{
	int		x;
	int		y;
	int		z;
	mobj_t* th;

	for (x = mo->x - 196 * FRACUNIT; x < mo->x + 320 * FRACUNIT; x += FRACUNIT * 8)
	{
		y = mo->y - 320 * FRACUNIT;
		z = 128 + P_Random(pr_brainscream) * 2 * FRACUNIT;
		th = P_SpawnMobj(x, y, z, MT_PROJ_ROCKET);
		th->momz = P_Random(pr_brainscream) * 512;

		P_SetMobjState(th, S_ROCKET_DIE1);

		th->tics -= P_Random(pr_brainscream) & 7;
		if (th->tics < 1)
			th->tics = 1;
	}

	S_StartSound(NULL, sfx_bosdth);
}

//
// A_BrainDie
//

void A_BrainDie(mobj_t* mo)
{
	G_ExitLevel();
}

//
// A_BrainSpit
//

void A_BrainSpit(mobj_t* mo)
{
	mobj_t* targ;
	mobj_t* newmobj;

	static int	easy = 0;

	easy ^= 1;
	if (gameskill <= sk_easy && (!easy))
		return;

	// shoot a cube at current target
	targ = braintargets[braintargeton];
	braintargeton = (braintargeton + 1) % numbraintargets;

	// spawn brain missile
	newmobj = P_SpawnMissile(mo, targ, MT_SPAWNSHOT, 0, 0, 0, true);
	newmobj->target = targ;
	newmobj->reactiontime =
		((targ->y - mo->y) / newmobj->momy) / newmobj->state->info_tics;

	S_StartSound(NULL, NULL);
}


//
// A_SpawnSound
//

void A_SpawnFly(mobj_t* mo);

// travelling cube sound
void A_SpawnSound(mobj_t* mo)
{
	S_StartSound(mo, NULL);
	A_SpawnFly(mo);
}

//
// A_SpawnFly
//

void A_SpawnFly(mobj_t* mo)
{
	mobj_t* newmobj;
	mobj_t* fog;
	mobj_t* targ;
	int		r;
	mobjtype_t	type;

	if (--mo->reactiontime)
		return;	// still flying

	targ = mo->target;

	// First spawn teleport fog.
	fog = P_SpawnMobj(targ->x, targ->y, targ->z, MT_SPAWNFIRE);
	S_StartSound(fog, sfx_telept);

	// Randomly select monster to spawn.
	r = P_Random(pr_spawnfly);

	// Probability distribution (kind of :),
	// decreasing likelihood.
	if (r < 50)
		type = MT_IMP1;
	else if (r < 90)
		type = MT_POSSESSED2;
	else if (r < 120)
		type = MT_DEMON2;
	else if (r < 130)
		type = MT_PAIN;
	else if (r < 160)
		type = MT_CACODEMON;
	else if (r < 162)
		type = MT_VILE;
	else if (r < 172)
		type = MT_UNDEAD;
	else if (r < 192)
		type = MT_BABY;
	else if (r < 222)
		type = MT_MANCUBUS;
	else if (r < 246)
		type = MT_BRUISER2;
	else
		type = MT_BRUISER1;

	newmobj = P_SpawnMobj(targ->x, targ->y, targ->z, type);
	if (P_LookForPlayers(newmobj, true))
		P_SetMobjState(newmobj, newmobj->info->seestate);

	// telefrag anything in this spot
	P_TeleportMove(newmobj, newmobj->x, newmobj->y);

	// remove self (i.e., cube).
	P_RemoveMobj(mo);
}

//
// A_RectChase2
//

void A_RectChase2(mobj_t* actor) {
	int			xl;
	int			xh;
	int			yl;
	int			yh;

	int			bx;
	int			by;

	mobjinfo_t* info;
	mobj_t* temp;

	if (actor->movedir != DI_NODIR)
	{
		// check for corpses to raise
		viletryx =
			actor->x + actor->info->speed * xspeed[actor->movedir];
		viletryy =
			actor->y + actor->info->speed * yspeed[actor->movedir];

		xl = (viletryx - bmaporgx - MAXRADIUS * 2) >> MAPBLOCKSHIFT;
		xh = (viletryx - bmaporgx + MAXRADIUS * 2) >> MAPBLOCKSHIFT;
		yl = (viletryy - bmaporgy - MAXRADIUS * 2) >> MAPBLOCKSHIFT;
		yh = (viletryy - bmaporgy + MAXRADIUS * 2) >> MAPBLOCKSHIFT;

		vileobj = actor;
		for (bx = xl; bx <= xh; bx++)
		{
			for (by = yl; by <= yh; by++)
			{
				// Call PIT_VileCheck to check
				// whether object is a corpse
				// that canbe raised.
				if (!P_BlockThingsIterator(bx, by, PIT_VileCheck))
				{
					// got one!
					temp = actor->target;
					actor->target = corpsehit;
					A_FaceTarget(actor);
					actor->target = temp;

					P_SetMobjState(actor, S_RECT2_HEAL1);
					S_StartSound(corpsehit, sfx_slop);
					info = corpsehit->info;

					P_SetMobjState(corpsehit, info->raisestate);
					corpsehit->height <<= 2;
					corpsehit->flags = info->flags;
					corpsehit->health = info->spawnhealth;
					corpsehit->target = NULL;

					return;
				}
			}
		}
	}



	// Return to normal attack.
	A_Chase(actor);
}

//
// A_RectGroundFire2
//

void A_RectGroundFire2(mobj_t* actor) {
	mobj_t* mo;
	angle_t an;

	if (!actor->target) {
		return;
	}

	A_FaceTarget(actor);

	mo = P_SpawnMobj(actor->x, actor->y, actor->z, MT_PROJ_RECTFIRE);
	P_SetTarget(&mo->target, actor);
	an = actor->angle + R_PointToAngle2(actor->x, actor->y, mo->target->x, mo->target->y);

	mo->angle = an;
	mo->angle >>= ANGLETOFINESHIFT;
	mo->momx = FixedMul(mo->info->speed, finecosine[mo->angle]);
	mo->momy = FixedMul(mo->info->speed, finesine[mo->angle]);
	mo->angle = an;

	mo = P_SpawnMobj(actor->x, actor->y, actor->z, MT_PROJ_RECTFIRE);
	P_SetTarget(&mo->target, actor);
	mo->angle = an - ANG45;
	mo->angle >>= ANGLETOFINESHIFT;
	mo->momx = FixedMul(mo->info->speed, finecosine[mo->angle]);
	mo->momy = FixedMul(mo->info->speed, finesine[mo->angle]);
	mo->angle = an - ANG45;

	mo = P_SpawnMobj(actor->x, actor->y, actor->z, MT_PROJ_RECTFIRE);
	P_SetTarget(&mo->target, actor);
	mo->angle = an + ANG45;
	mo->angle >>= ANGLETOFINESHIFT;
	mo->momx = FixedMul(mo->info->speed, finecosine[mo->angle]);
	mo->momy = FixedMul(mo->info->speed, finesine[mo->angle]);
	mo->angle = an + ANG45;

	S_StartSound(mo, mo->info->seesound);
	S_StartSound(actor, sfx_rectatk);
}

//
// A_RectMissile2
//

void A_RectMissile2(mobj_t* actor) {
	mobj_t* mo;
	int count = 0;
	angle_t an = 0;
	fixed_t x = 0;
	fixed_t y = 0;

	if (!actor->target) {
		return;
	}

	A_FaceTarget(actor);
	for (mo = mobjhead.next; mo != &mobjhead; mo = mo->next) {
		// not a rect projectile
		if (mo->type != MT_PROJ_RECT) {
			continue;
		}

		count++;
	}

	if (!(count < 9)) {
		return;
	}

	// Arm 1

	an = (actor->angle - ANG90) >> ANGLETOFINESHIFT;
	x = FixedMul(68 * FRACUNIT, finecosine[an]);
	y = FixedMul(68 * FRACUNIT, finesine[an]);
	mo = P_SpawnMobj(actor->x + x, actor->y + y, actor->z + 68 * FRACUNIT, MT_PROJ_RECT);
	P_SetTarget(&mo->target, actor);
	P_SetTarget(&mo->tracer, actor->target);
	mo->threshold = 5;
	an = (actor->angle + ANG270);
	mo->angle = an;
	an >>= ANGLETOFINESHIFT;
	mo->momx = FixedMul(mo->info->speed, finecosine[an]);
	mo->momy = FixedMul(mo->info->speed, finesine[an]);

	// Arm2

	an = (actor->angle - ANG90) >> ANGLETOFINESHIFT;
	x = FixedMul(50 * FRACUNIT, finecosine[an]);
	y = FixedMul(50 * FRACUNIT, finesine[an]);
	mo = P_SpawnMobj(actor->x + x, actor->y + y, actor->z + 139 * FRACUNIT, MT_PROJ_RECT);
	P_SetTarget(&mo->target, actor);
	P_SetTarget(&mo->tracer, actor->target);
	mo->threshold = 1;
	an = (actor->angle + ANG270);
	mo->angle = an;
	an >>= ANGLETOFINESHIFT;
	mo->momx = FixedMul(mo->info->speed, finecosine[an]);
	mo->momy = FixedMul(mo->info->speed, finesine[an]);

	// Arm3

	an = (actor->angle + ANG90) >> ANGLETOFINESHIFT;
	x = FixedMul(68 * FRACUNIT, finecosine[an]);
	y = FixedMul(68 * FRACUNIT, finesine[an]);
	mo = P_SpawnMobj(actor->x + x, actor->y + y, actor->z + 68 * FRACUNIT, MT_PROJ_RECT);
	P_SetTarget(&mo->target, actor);
	P_SetTarget(&mo->tracer, actor->target);
	mo->threshold = 5;
	an = (actor->angle - ANG270);
	mo->angle = an;
	an >>= ANGLETOFINESHIFT;
	mo->momx = FixedMul(mo->info->speed, finecosine[an]);
	mo->momy = FixedMul(mo->info->speed, finesine[an]);

	// Arm4

	an = (actor->angle + ANG90) >> ANGLETOFINESHIFT;
	x = FixedMul(50 * FRACUNIT, finecosine[an]);
	y = FixedMul(50 * FRACUNIT, finesine[an]);
	mo = P_SpawnMobj(actor->x + x, actor->y + y, actor->z + 139 * FRACUNIT, MT_PROJ_RECT);
	P_SetTarget(&mo->target, actor);
	P_SetTarget(&mo->tracer, actor->target);
	mo->threshold = 1;
	an = (actor->angle - ANG270);
	mo->angle = an;
	an >>= ANGLETOFINESHIFT;
	mo->momx = FixedMul(mo->info->speed, finecosine[an]);
	mo->momy = FixedMul(mo->info->speed, finesine[an]);
}

//
// A_HectAttack1
//

void A_HectAttack1(mobj_t* actor) {
	mobj_t* mo;
	angle_t an;

	if (!actor->target) {
		return;
	}

	A_FaceTarget(actor);
	P_MissileAttack(actor, DP_RIGHT);
	mo = P_SpawnMissile(actor, actor->target, MT_PROJ_HECTEBUS, 0, 0, 69, true);
	mo->angle += FATSPREAD / 2;
	an = mo->angle >> ANGLETOFINESHIFT;
	mo->momx = FixedMul(mo->info->speed, finecosine[an]);
	mo->momy = FixedMul(mo->info->speed, finesine[an]);

	mo = P_SpawnMissile(actor, actor->target, MT_PROJ_HECTEBUS, 0, 0, 69, true);
	mo->angle += FATSPREAD;
	an = mo->angle >> ANGLETOFINESHIFT;
	mo->momx = FixedMul(mo->info->speed, finecosine[an]);
	mo->momy = FixedMul(mo->info->speed, finesine[an]);

	mo = P_SpawnMissile(actor, actor->target, MT_PROJ_HECTEBUS, 0, 0, 69, true);
	mo->angle += FATSPREAD * 3 / 2;
	an = mo->angle >> ANGLETOFINESHIFT;
	mo->momx = FixedMul(mo->info->speed, finecosine[an]);
	mo->momy = FixedMul(mo->info->speed, finesine[an]);
}

//
// A_HectAttack2
//

void A_HectAttack2(mobj_t* actor) {
	mobj_t* mo;
	angle_t an;

	if (!actor->target) {
		return;
	}

	A_FaceTarget(actor);
	P_MissileAttack(actor, DP_LEFT);
	mo = P_SpawnMissile(actor, actor->target, MT_PROJ_HECTEBUS, 0, 0, 69, true);
	mo->angle -= FATSPREAD / 2;
	an = mo->angle >> ANGLETOFINESHIFT;
	mo->momx = FixedMul(mo->info->speed, finecosine[an]);
	mo->momy = FixedMul(mo->info->speed, finesine[an]);

	mo = P_SpawnMissile(actor, actor->target, MT_PROJ_HECTEBUS, 0, 0, 69, true);
	mo->angle -= FATSPREAD;
	an = mo->angle >> ANGLETOFINESHIFT;
	mo->momx = FixedMul(mo->info->speed, finecosine[an]);
	mo->momy = FixedMul(mo->info->speed, finesine[an]);

	mo = P_SpawnMissile(actor, actor->target, MT_PROJ_HECTEBUS, 0, 0, 69, true);
	mo->angle -= FATSPREAD * 3 / 2;
	an = mo->angle >> ANGLETOFINESHIFT;
	mo->momx = FixedMul(mo->info->speed, finecosine[an]);
	mo->momy = FixedMul(mo->info->speed, finesine[an]);
}

//
// A_HectAttack3
//

void A_HectAttack3(mobj_t* actor) {
	mobj_t* mo;
	angle_t an;

	if (!actor->target) {
		return;
	}

	A_FaceTarget(actor);
	P_MissileAttack(actor, DP_RIGHT);
	P_MissileAttack(actor, DP_LEFT);
	mo = P_SpawnMissile(actor, actor->target, MT_PROJ_HECTEBUS, 0, 0, 69, true);
	mo->angle += FATSPREAD / 2;
	an = mo->angle >> ANGLETOFINESHIFT;
	mo->momx = FixedMul(mo->info->speed, finecosine[an]);
	mo->momy = FixedMul(mo->info->speed, finesine[an]);

	mo = P_SpawnMissile(actor, actor->target, MT_PROJ_HECTEBUS, 0, 0, 69, true);
	mo->angle -= FATSPREAD / 2;
	an = mo->angle >> ANGLETOFINESHIFT;
	mo->momx = FixedMul(mo->info->speed, finecosine[an]);
	mo->momy = FixedMul(mo->info->speed, finesine[an]);

	mo = P_SpawnMissile(actor, actor->target, MT_PROJ_HECTEBUS, 0, 0, 69, true);
	mo->angle += FATSPREAD;
	an = mo->angle >> ANGLETOFINESHIFT;
	mo->momx = FixedMul(mo->info->speed, finecosine[an]);
	mo->momy = FixedMul(mo->info->speed, finesine[an]);

	mo = P_SpawnMissile(actor, actor->target, MT_PROJ_HECTEBUS, 0, 0, 69, true);
	mo->angle -= FATSPREAD;
	an = mo->angle >> ANGLETOFINESHIFT;
	mo->momx = FixedMul(mo->info->speed, finecosine[an]);
	mo->momy = FixedMul(mo->info->speed, finesine[an]);
}

//
// A_PainElementalNightmareChase
// Check for ressurecting a body
//
void A_PainElementalNightmareChase(mobj_t* actor)
{
	int			xl;
	int			xh;
	int			yl;
	int			yh;

	int			bx;
	int			by;

	mobjinfo_t* info;
	mobj_t* temp;

	if (actor->movedir != DI_NODIR)
	{
		// check for corpses to raise
		viletryx =
			actor->x + actor->info->speed * xspeed[actor->movedir];
		viletryy =
			actor->y + actor->info->speed * yspeed[actor->movedir];

		xl = (viletryx - bmaporgx - MAXRADIUS * 2) >> MAPBLOCKSHIFT;
		xh = (viletryx - bmaporgx + MAXRADIUS * 2) >> MAPBLOCKSHIFT;
		yl = (viletryy - bmaporgy - MAXRADIUS * 2) >> MAPBLOCKSHIFT;
		yh = (viletryy - bmaporgy + MAXRADIUS * 2) >> MAPBLOCKSHIFT;

		vileobj = actor;
		for (bx = xl; bx <= xh; bx++)
		{
			for (by = yl; by <= yh; by++)
			{
				// Call PIT_VileCheck to check
				// whether object is a corpse
				// that canbe raised.
				if (!P_BlockThingsIterator(bx, by, PIT_VileCheck))
				{
					// got one!
					temp = actor->target;
					actor->target = corpsehit;
					A_FaceTarget(actor);
					actor->target = temp;

					P_SetMobjState(actor, S_PAIG_HEAL1);
					S_StartSound(corpsehit, sfx_slop);
					info = corpsehit->info;

					P_SetMobjState(corpsehit, info->raisestate);
					corpsehit->height <<= 2;
					corpsehit->flags = info->flags;
					corpsehit->health = info->spawnhealth;
					corpsehit->target = NULL;

					return;
				}
			}
		}
	}

	// Return to normal attack.
	A_Chase(actor);
}

//
// A_PainElementalNightmareDecide
//

void A_PainElementalNightmareDecide(mobj_t* actor)
{
	if (P_Random(pr_painelementalnightmaredecide) < 128)
	{
		P_SetMobjState(actor, S_PAIG_ATK1_1);
	}
	else if (P_Random(pr_painelementalnightmaredecide) < 256)
	{
		P_SetMobjState(actor, S_PAIG_ATK2_1);
	}
}

//
// A_PainElementalNightmareShootSkull
// Spawn a lost soul and launch it at the target
//

void A_PainElementalNightmareShootSkull(mobj_t* actor, angle_t angle) {
	fixed_t     x;
	fixed_t     y;
	fixed_t     z;
	mobj_t* newmobj;
	angle_t     an;
	int         prestep;
	int         count;

	// count total number of skull currently on the level
	count = 0;

	for (newmobj = mobjhead.next; newmobj != &mobjhead; newmobj = newmobj->next) {
		if (newmobj->type == MT_NIGHTMARE_LOSTSOUL) {
			count++;
		}
	}

	// if there are all ready 17 skulls on the level, don't spit another one
	if (count >= 17) {
		return;
	}

	an = angle >> ANGLETOFINESHIFT;

	prestep = 4 * FRACUNIT + 3 * (actor->info->radius + mobjinfo[MT_NIGHTMARE_LOSTSOUL].radius) / 2;

	x = actor->x + FixedMul(prestep, finecosine[an]);
	y = actor->y + FixedMul(prestep, finesine[an]);
	z = actor->z + 16 * FRACUNIT;

	newmobj = P_SpawnMobj(x, y, z, MT_NIGHTMARE_LOSTSOUL);

	// Check for movements

	if ((!P_TryMove(newmobj, newmobj->x, newmobj->y)) ||
		(!P_PathTraverse(actor->x, actor->y, newmobj->x, newmobj->y, PT_ADDLINES, PIT_PainCheckLine))) {
		// kill it immediately

		P_DamageMobj(newmobj, actor, actor, 10000);
		P_RadiusAttack(newmobj, newmobj, 128);
		return;
	}

	P_SetTarget(&newmobj->target, actor->target);
	P_SetMobjState(newmobj, newmobj->info->missilestate);
	A_SkullAttack(newmobj);
}

//
// A_PainElementalNightmareAttack
// Spawn a lost soul and launch it at the target
//

void A_PainElementalNightmareAttack(mobj_t* actor) {
	if (!actor->target) {
		return;
	}

	A_FaceTarget(actor);
	A_PainElementalNightmareShootSkull(actor, actor->angle + 0x15550000);
	A_PainElementalNightmareShootSkull(actor, actor->angle - 0x15550000);
}

//
// A_PainElementalNightmareAttack2
// 
//

void A_PainElementalNightmareAttack2(mobj_t* actor) {
	if (!actor->target) {
		return;
	}

	A_FaceTarget(actor);
	P_MissileAttack(actor, DP_RIGHT);
	P_MissileAttack(actor, DP_LEFT);
}

//
// A_PainElementalNightmareDie
//

void A_PainElementalNightmareDie(mobj_t* actor) {
	A_Fall(actor);
	A_PainElementalNightmareShootSkull(actor, actor->angle + ANG90);
	A_PainElementalNightmareShootSkull(actor, actor->angle + ANG180);
	A_PainElementalNightmareShootSkull(actor, actor->angle + ANG270);

	A_OnDeathTrigger(actor);
}

//
// A_PlasmaZombieAttack
//

void A_PlasmaZombieAttack(mobj_t* actor) {
	if (!actor->target) {
		return;
	}

	A_FaceTarget(actor);
	P_MissileAttack(actor, DP_LEFT);
}

//
// A_BFGCommandoRaise
//


void A_BFGCommandoRaise(mobj_t* actor) {
	if (!actor->target) {
		return;
	}

	A_FaceTarget(actor);
	S_StartSound(actor, sfx_bfg);
}

//
// A_BFGCommandoAttack
//

void A_BFGCommandoAttack(mobj_t* actor) {
	if (!actor->target) {
		return;
	}

	A_FaceTarget(actor);
	P_MissileAttack(actor, DP_RIGHT);
}

//
// A_BFGCyberAttack
//

void A_BFGCyberAttack(mobj_t* actor) {
	if (!actor->target) {
		return;
	}

	A_FaceTarget(actor);
	P_MissileAttack(actor, DP_LEFT);
	S_StartSound(actor, sfx_bfg);
}

//
// A_StalkerDecide
//

void A_StalkerDecide(mobj_t* actor)
{
	if (P_Random(pr_stalkerdecide) < 85)
	{
		P_SetMobjState(actor, S_STLK_ATK1_1);
	}
	else if (P_Random(pr_stalkerdecide) < 170)
	{
		P_SetMobjState(actor, S_STLK_ATK2_1);
	}
	else if (P_Random(pr_stalkerdecide) < 256)
	{
		P_SetMobjState(actor, S_STLK_ATK3_1);
	}
}

//
// A_StalkerMissile
//

void A_StalkerMissile(mobj_t* actor, int direction)
{
	mobj_t* mo;
	angle_t angle;

	if (direction == DP_LEFT) {
		angle = actor->angle + ANG45;
	}
	else if (direction == DP_RIGHT) {
		angle = actor->angle - ANG45;
	}
	else {
		angle = actor->angle;
	}
	angle >>= ANGLETOFINESHIFT;

	mo = P_SpawnMissile(actor,
		actor->target,
		MT_PROJ_STALKER2,
		FixedMul(26 * FRACUNIT, finecosine[angle]),
		FixedMul(26 * FRACUNIT, finesine[angle]),
		32,
		true);
	mo->x += mo->momx;
	mo->y += mo->momy;
	mo->tracer = actor->target;
}

//
// A_StalkerAttack1
//

void A_StalkerAttack1(mobj_t* actor) {
	if (!actor->target) {
		return;
	}

	A_FaceTarget(actor);
	P_MissileAttack(actor, DP_STRAIGHT);
}

//
// A_StalkerAttack2
//

void A_StalkerAttack2(mobj_t* actor) {
	if (!actor->target) {
		return;
	}

	A_FaceTarget(actor);
	A_StalkerMissile(actor, DP_STRAIGHT);
}

//
// A_PainElementalStalkerShootSkull
// Spawn a lost soul and launch it at the target
//

void A_PainElementalStalkerShootSkull(mobj_t* actor, angle_t angle) {
	fixed_t     x;
	fixed_t     y;
	fixed_t     z;
	mobj_t* newmobj;
	angle_t     an;
	int         prestep;
	int         count;

	// count total number of skull currently on the level
	count = 0;

	for (newmobj = mobjhead.next; newmobj != &mobjhead; newmobj = newmobj->next) {
		if (newmobj->type == MT_STALKER) {
			count++;
		}
	}

	// if there are all ready 17 skulls on the level, don't spit another one
	if (count >= 17) {
		return;
	}

	an = angle >> ANGLETOFINESHIFT;

	prestep = 4 * FRACUNIT + 3 * (actor->info->radius + mobjinfo[MT_STALKER].radius) / 2;

	x = actor->x + FixedMul(prestep, finecosine[an]);
	y = actor->y + FixedMul(prestep, finesine[an]);
	z = actor->z + 16 * FRACUNIT;

	newmobj = P_SpawnMobj(x, y, z, MT_STALKER);

	// Check for movements

	if ((!P_TryMove(newmobj, newmobj->x, newmobj->y)) ||
		(!P_PathTraverse(actor->x, actor->y, newmobj->x, newmobj->y, PT_ADDLINES, PIT_PainCheckLine))) {
		// kill it immediately

		P_DamageMobj(newmobj, actor, actor, 10000);
		P_RadiusAttack(newmobj, newmobj, 128);
		return;
	}

	P_SetTarget(&newmobj->target, actor->target);
	P_SetMobjState(newmobj, S_STLK_ATK3_1);
	A_SkullAttack(newmobj);
}

//
// A_PainElementalStalkerAttack
// Spawn a lost soul and launch it at the target
//

void A_PainElementalStalkerAttack(mobj_t* actor) {
	if (!actor->target) {
		return;
	}

	A_FaceTarget(actor);
	A_PainElementalStalkerShootSkull(actor, actor->angle + 0x15550000);
	A_PainElementalStalkerShootSkull(actor, actor->angle - 0x15550000);
}

//
// A_PainElementalStalkerDie
//

void A_PainElementalStalkerDie(mobj_t* actor) {
	A_Fall(actor);
	A_PainElementalStalkerShootSkull(actor, actor->angle + ANG90);
	A_PainElementalStalkerShootSkull(actor, actor->angle + ANG180);
	A_PainElementalStalkerShootSkull(actor, actor->angle + ANG270);

	A_OnDeathTrigger(actor);
}

//
// A_RevenantNightmareMissile
//

void A_RevenantNightmareMissile(mobj_t* actor, int direction)
{
	mobj_t* mo;
	angle_t angle;

	if (direction == DP_LEFT) {
		angle = actor->angle + ANG45;
	}
	else if (direction == DP_RIGHT) {
		angle = actor->angle - ANG45;
	}
	else {
		angle = actor->angle;
	}
	angle >>= ANGLETOFINESHIFT;

	mo = P_SpawnMissile(actor,
		actor->target,
		MT_PROJ_NIGHTMARE_REVENANT,
		FixedMul(26 * FRACUNIT, finecosine[angle]),
		FixedMul(26 * FRACUNIT, finesine[angle]),
		104,
		true);
	mo->x += mo->momx;
	mo->y += mo->momy;
	mo->tracer = actor->target;
}

//
// A_RevenantNightmareAttack
//

void A_RevenantNightmareAttack(mobj_t* actor)
{
	if (!actor->target)
		return;
	A_FaceTarget(actor);
	A_RevenantNightmareMissile(actor, DP_LEFT);
	A_RevenantNightmareMissile(actor, DP_RIGHT);
}

//
// A_NightmareLostSoulChase
// Check for ressurecting a body
//
void A_NightmareLostSoulChase(mobj_t* actor)
{
	int			xl;
	int			xh;
	int			yl;
	int			yh;

	int			bx;
	int			by;

	mobjinfo_t* info;
	mobj_t* temp;

	if (actor->movedir != DI_NODIR)
	{
		// check for corpses to raise
		viletryx =
			actor->x + actor->info->speed * xspeed[actor->movedir];
		viletryy =
			actor->y + actor->info->speed * yspeed[actor->movedir];

		xl = (viletryx - bmaporgx - MAXRADIUS * 2) >> MAPBLOCKSHIFT;
		xh = (viletryx - bmaporgx + MAXRADIUS * 2) >> MAPBLOCKSHIFT;
		yl = (viletryy - bmaporgy - MAXRADIUS * 2) >> MAPBLOCKSHIFT;
		yh = (viletryy - bmaporgy + MAXRADIUS * 2) >> MAPBLOCKSHIFT;

		vileobj = actor;
		for (bx = xl; bx <= xh; bx++)
		{
			for (by = yl; by <= yh; by++)
			{
				// Call PIT_VileCheck to check
				// whether object is a corpse
				// that canbe raised.
				if (!P_BlockThingsIterator(bx, by, PIT_VileCheck))
				{
					// got one!
					temp = actor->target;
					actor->target = corpsehit;
					A_FaceTarget(actor);
					actor->target = temp;

					P_SetMobjState(actor, S_SKUG_HEAL1);
					S_StartSound(corpsehit, sfx_slop);
					info = corpsehit->info;

					P_SetMobjState(corpsehit, info->raisestate);
					corpsehit->height <<= 2;
					corpsehit->flags = info->flags;
					corpsehit->health = info->spawnhealth;
					corpsehit->target = NULL;

					return;
				}
			}
		}
	}

	// Return to normal attack.
	A_Chase(actor);
}

//
// A_ArthronailerAttack
//

void A_ArthronailerAttack(mobj_t* actor) {
	if (!actor->target) {
		return;
	}

	A_FaceTarget(actor);
	P_MissileAttack(actor, DP_LEFT);
	P_MissileAttack(actor, DP_RIGHT);
	S_StartSound(actor, sfx_nailgun);
}