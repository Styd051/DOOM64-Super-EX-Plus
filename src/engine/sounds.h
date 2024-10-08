// Emacs style mode select   -*- C -*-
//-----------------------------------------------------------------------------
//
// Copyright(C) 1993-1997 Id Software, Inc.
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

#ifndef __SOUNDS__
#define __SOUNDS__

//
// Identifiers for all sfx in game.
//

typedef enum {
	sfx_None,
	sfx_punch,      //punch
	sfx_spawn,      //spawn
	sfx_explode,    //explode
	sfx_implod,     //impact
	sfx_pistol,     //pistol
	sfx_shotgun,    //shotgun
	sfx_plasma,     //plasma
	sfx_bfg,        //bfg
	sfx_sawup,      //sawup
	sfx_sawidle,    //sawidle
	sfx_saw1,       //saw1
	sfx_saw2,       //saw2
	sfx_missile,    //missile
	sfx_bfgexp,     //bfgexplode
	sfx_pstart,     //platup
	sfx_pstop,      //platdown
	sfx_doorup,     //doorup
	sfx_doordown,   //doordown
	sfx_secmove,    //secmove
	sfx_switch1,    //switch1
	sfx_switch2,    //switch2
	sfx_itemup,     //itemup
	sfx_sgcock,     //sgcock
	sfx_oof,        //oof
	sfx_telept,     //teleport
	sfx_noway,      //oof2??
	sfx_sht2fire,   //shot2fire
	sfx_sht2load1,  //shot2load1
	sfx_sht2load2,  //shot2load2
	sfx_sht2load3,  //shot2load2
	sfx_plrpain,    //playerpain
	sfx_plrdie,     //playerdie
	sfx_slop,       //slop
	sfx_possit1,    //posssit1
	sfx_possit2,    //posssit2
	sfx_possit3,    //posssit3
	sfx_posdie1,    //possdie1
	sfx_posdie2,    //possdie2
	sfx_posdie3,    //possdie3
	sfx_posact,     //possact
	sfx_dbpain1,    //pain1
	sfx_dbpain2,    //pain2
	sfx_dbact,      //monsteract
	sfx_scratch,    //scratch
	sfx_impsit1,    //impsit1
	sfx_impsit2,    //impsit2
	sfx_impdth1,    //impdeath1
	sfx_impdth2,    //impdeath2
	sfx_impact,     //impact
	sfx_sargsit,    //sargsit
	sfx_sargatk,    //sargatk
	sfx_sargdie,    //sargdie
	sfx_bos1sit,    //boss1sit
	sfx_bos1die,    //boss1die
	sfx_headsit,    //headsit
	sfx_headdie,    //headdie
	sfx_skullatk,    //skullatk
	sfx_bos2sit,    //boss2sit
	sfx_bos2die,    //boss2die
	sfx_pesit,      //painsit
	sfx_pepain,     //painhit
	sfx_pedie,      //paindie
	sfx_bspisit,    //bspisit
	sfx_bspidie,    //bspidie
	sfx_bspilift,   //bspilift
	sfx_bspistomp,  //bspistomp
	sfx_fattatk,    //fattatk
	sfx_fattsit,    //fattsit
	sfx_fatthit,    //fatthit
	sfx_fattdie,    //fattdie
	sfx_bdmissile,  //projectile
	sfx_skelact,    //revenantact??
	sfx_tracer,     //tracer
	sfx_dart,       //dart
	sfx_dartshoot,  //revenantpunch??
	sfx_cybsit,     //cybsit
	sfx_cybdth,     //cybdeath
	sfx_cybhoof,    //hoof
	sfx_metal,      //metal
	sfx_door2up,    //door2up
	sfx_door2dwn,   //door2down
	sfx_powerup,    //powerup
	sfx_laser,      //laser
	sfx_electric,   //electric (loop)
	sfx_thndrlow,   //thunderlow
	sfx_thndrhigh,  //thunderhigh
	sfx_quake,      //quake (loop)
	sfx_darthit,    //darthit
	sfx_rectact,    //rectact
	sfx_rectatk,    //rectatk
	sfx_rectdie,    //rectdie
	sfx_rectpain,   //rectpain
	sfx_rectsit,    //rectsit
	sfx_skelsit,
	sfx_skeldth,
	sfx_spisit,
	sfx_spidth,
	sfx_spistomp,  //spistomp
	sfx_annsit,     //annsit
	sfx_anndth,     //anndeath
	sfx_annhoof,    //annhoof
	sfx_gechsit,    //gechsit
	sfx_gechatk,    //gechatk
	sfx_gechdie,    //gechdie
	sfx_dkhlatk,    //dkhlatk
	sfx_dkhlsit,    //dkhlsit
	sfx_dkhlhit,    //dkhlhit
	sfx_dkhldie,    //dkhldie
	sfx_br64sit,    //br64sit
	sfx_br64die,    //br64die
	sfx_vilatk,
	sfx_vilsit,
	sfx_vipain,
	sfx_vildth,
	sfx_vilact,
	sfx_flamst,
	sfx_flame,
	sfx_quaddamageact,
	sfx_quaddamageatt,
	sfx_nailgun,
	sfx_nailtink,
	sfx_bossit,
	sfx_bospn,
	sfx_bosdth,
	sfx_keenpn,
	sfx_keendt,
	sfx_sssit,
	sfx_ssdth,
	sfx_belsit,
	sfx_belpain,
	sfx_beldie,
	sfx_belact,
	sfx_stlksit,
	sfx_stlkpain,
	sfx_quadshotgunfire,
	sfx_quadshotgunopen,
	sfx_quadshotgunload,
	sfx_quadshotgunclose,
	sfx_tsit1,
	sfx_tsit2,
	sfx_tpain,
	sfx_tdie,
	sfx_tapocast,
	sfx_gruntsit,
	sfx_gruntpain,
	sfx_gruntdie,
	sfx_ferrysee,
	sfx_ferrypain,
	sfx_ferrydie,
	sfx_ferrylaugh,
	sfx_chnsit,
	sfx_chnatk,
	sfx_chnpain,
	sfx_chndie,
	sfx_chnact,
	sfx_spcpain,
	sfx_spcact,
	sfx_imppain,
	sfx_headpain,    
	sfx_headact,
	sfx_kntpain,
	sfx_kntact,
	sfx_brpain,
	sfx_bract,
	sfx_manact,
	sfx_peact,
	sfx_bsppain,
	sfx_skepain,
	sfx_cybpain,
	sfx_cybact,
	sfx_spipain1,
	sfx_spipain2,
	sfx_hexashotgunfire,
	sfx_centaursit,
	sfx_centauratk,
	sfx_centaurpain,
	sfx_centaurdie,
	sfx_centauract,
	sfx_centaurmissilesit,
	sfx_centaurmissiledie,
	sfx_bfg10kelectric, //bfg10kelectric (loop)
	sfx_bfg10kf,
	sfx_bfg10kcool,
	sfx_wizardsit,
	sfx_wizardatk,
	sfx_wizardpain,
	sfx_wizarddie,
	sfx_wizardact,
	sfx_darknotronsit,
	sfx_darknotrondie,
	sfx_darknotronact,
	sfx_darknotronstomp,
	sfx_darknotronplasma,
	sfx_deeponesit,
	sfx_deeponeatk1,
	sfx_deeponeatk2,
	sfx_deeponepain1,
	sfx_deeponepain2,
	sfx_deeponepain3,
	sfx_deeponepain4,
	sfx_deeponedie,
	sfx_deeponeact1,
	sfx_deeponeact2,
	sfx_deeponefire,
	sfx_deeponehit,
	sfx_flamefire1,
	sfx_flamefire2,
	sfx_flameburn,
	sfx_flamedie1,
	sfx_flamedie2,
	sfx_flamedie3,
	mus_amb01,
	mus_amb02,
	mus_amb03,
	mus_amb04,
	mus_amb05,
	mus_amb06,
	mus_amb07,
	mus_amb08,
	mus_amb09,
	mus_amb10,
	mus_amb11,
	mus_amb12,
	mus_amb13,
	mus_amb14,
	mus_amb15,
	mus_amb16,
	mus_amb17,
	mus_amb18,
	mus_amb19,
	mus_amb20,
	mus_amb21,
	mus_amb22,
	mus_amb23,
	mus_amb24,
	mus_amb25,
	mus_amb26,
	mus_amb27,
	mus_amb28,
	mus_amb29,
	mus_amb30,
	mus_final,
	mus_complete,
	mus_intro,
	mus_title,
	NUMSFX
} sfxenum_t;

#endif
