//       _________ __                 __
//      /   _____//  |_____________ _/  |______     ____  __ __  ______
//      \_____  \\   __\_  __ \__  \\   __\__  \   / ___\|  |  \/  ___/
//      /        \|  |  |  | \// __ \|  |  / __ \_/ /_/  >  |  /\___ |
//     /_______  /|__|  |__|  (____  /__| (____  /\___  /|____//____  >
//             \/                  \/          \//_____/            \/
//  ______________________                           ______________________
//                        T H E   W A R   B E G I N S
//         Stratagus - A free fantasy real time strategy game engine
//
/**@name map_radar.c - The map radar handling. */
//
//      (c) Copyright 1999-2004 by Russell Smith.
//
//      This program is free software; you can redistribute it and/or modify
//      it under the terms of the GNU General Public License as published by
//      the Free Software Foundation; only version 2 of the License.
//
//      This program is distributed in the hope that it will be useful,
//      but WITHOUT ANY WARRANTY; without even the implied warranty of
//      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//      GNU General Public License for more details.
//
//      You should have received a copy of the GNU General Public License
//      along with this program; if not, write to the Free Software
//      Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
//      02111-1307, USA.
//
//      $Id$

//@{

/*----------------------------------------------------------------------------
--  Includes
----------------------------------------------------------------------------*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "stratagus.h"
#include "player.h"
#include "unittype.h"
#include "unit.h"
#include "map.h"
#include "tileset.h"
#include "minimap.h"
#include "font.h"
#include "ui.h"

/*----------------------------------------------------------------------------
--  Functions
----------------------------------------------------------------------------*/

/**
**  Find out if a unit is visible under radar (By player, or by shared vision)
**
**  @param pradar  Player to who has radar.
**  @param punit   Unit to check for.
**
**  @return        0 jammed or not radar visible, >0 radar visible.
*/
unsigned char UnitVisibleOnRadar(const Player* pradar, const Unit* punit)
{
	int i;
	int j;

	for (i = punit->X; i < punit->X + punit->Type->TileWidth; ++i) {
		for (j = punit->Y; j < punit->Y + punit->Type->TileHeight; ++j) {
			if (IsTileRadarVisible(pradar, punit->Player, i, j) != 0) {
				return 1;
			}
		}
	}

	// Can't exit till the end, as we might be be able to see a different tile
	return 0;
}

/**
**  Find out if a unit is visible under radar (By player, or by shared vision)
**
**  @param pradar  Player to who has radar.
**  @param punit   Player who is being check.
**  @param x       x tile location to check.
**  @param y       y tile location to check.
**
**  @return        0 jammed or not radar visible, >0 radar visible.
*/
unsigned char IsTileRadarVisible(const Player* pradar, const Player* punit, int x, int y)
{
	int i;
	unsigned char radarvision;
	unsigned char jammingvision;
	unsigned char* radar;
	unsigned char* jamming;

	radar = TheMap.Fields[y * TheMap.Info.MapWidth + x].Radar;
	jamming = TheMap.Fields[y * TheMap.Info.MapWidth + x].RadarJammer;
	radarvision = radar[pradar->Player];
	jammingvision = jamming[punit->Player];

	if (jammingvision) {
		return 0;
	}
	if (!pradar->SharedVision) {
		return radarvision;
	}

	// Check Jamming first, if we are jammed, exit
	for (i = 0; i < PlayerMax ; ++i) {
		if (jamming[i] > 0 && punit->SharedVision & (1 << i) &&
				(Players[i].SharedVision & (1 << punit->Player))) {
			// We are jammed, return nothing
			return 0;
		}
		if (pradar->SharedVision & (1 << i) &&
				(Players[i].SharedVision & (1 << pradar->Player))) {
			radarvision |= radar[i];
		}
	}

	// Can't exit till the end, as we might be jammed
	return radarvision;
}

/*
**  Mark Radar Vision for a tile
**
**  @param player  The player you are marking for
**  @param x       the X tile to mark.
**  @param y       the Y tile to mark.
**
*/
void MapMarkTileRadar(const Player* player, int x, int y)
{
	unsigned char v;

	Assert(0 <= x && x < TheMap.Info.MapWidth);
	Assert(0 <= y && y < TheMap.Info.MapHeight);
	v = TheMap.Fields[x + y * TheMap.Info.MapWidth].Radar[player->Player];
	switch (v) {
		case 255:  // Overflow
			DebugPrint("Radar overflow (Player): %d\n" _C_ player->Player);
			break;
		default:
			++v;
			break;
	}
	TheMap.Fields[x + y * TheMap.Info.MapWidth].Radar[player->Player] = v;
}

/*
**  Unmark Radar Vision for a tile
**
**  @param player  The player you are marking for
**  @param x       the X tile to mark.
**  @param y       the Y tile to mark.
**
*/
void MapUnmarkTileRadar(const Player* player, int x, int y)
{
	unsigned char v;

	Assert(0 <= x && x < TheMap.Info.MapWidth);
	Assert(0 <= y && y < TheMap.Info.MapHeight);
	v = TheMap.Fields[x + y * TheMap.Info.MapWidth].Radar[player->Player];
	// Reduce radar coverage if it exists.
	if (v == 255) {
		// FIXME: (mr-russ) Add radar counter for unit
		DebugPrint("Radar overflowed\n");
		Assert(1);
	}
	if (v) {
		--v;
	}
	TheMap.Fields[x + y * TheMap.Info.MapWidth].Radar[player->Player] = v;
}

/*
**  Mark Radar Jamming Vision for a tile
**
**  @param player  The player you are marking for
**  @param x       the X tile to mark.
**  @param y       the Y tile to mark.
**
*/
void MapMarkTileRadarJammer(const Player* player, int x, int y)
{
	unsigned char v;

	Assert(0 <= x && x < TheMap.Info.MapWidth);
	Assert(0 <= y && y < TheMap.Info.MapHeight);
	v = TheMap.Fields[x + y * TheMap.Info.MapWidth].RadarJammer[player->Player];
	switch (v) {
		case 255:  // Overflow
			DebugPrint("Radar Jamming overflow (Player): %d\n" _C_ player->Player);
			break;
		default:
			++v;
			break;
	}
	TheMap.Fields[x + y * TheMap.Info.MapWidth].RadarJammer[player->Player] = v;
}

/*
**  Unmark Radar Vision for a tile
**
**  @param player  The player you are marking for
**  @param x       the X tile to mark.
**  @param y       the Y tile to mark.
**
*/
void MapUnmarkTileRadarJammer(const Player* player, int x, int y)
{
	unsigned char v;

	Assert(0 <= x && x < TheMap.Info.MapWidth);
	Assert(0 <= y && y < TheMap.Info.MapHeight);
	v = TheMap.Fields[x + y * TheMap.Info.MapWidth].RadarJammer[player->Player];
	// Reduce radar coverage if it exists.
	if (v) {
		--v;
	}
	TheMap.Fields[x + y * TheMap.Info.MapWidth].RadarJammer[player->Player] = v;
}
