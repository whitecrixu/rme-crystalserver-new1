//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////
// Remere's Map Editor is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// Remere's Map Editor is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <http://www.gnu.org/licenses/>.
//////////////////////////////////////////////////////////////////////

#include "main.h"

#include "spawn_monster_brush.h"
#include "basemap.h"
#include "spawn_monster.h"
#include "monster_brush.h"

//=============================================================================
// SpawnMonster brush

SpawnMonsterBrush::SpawnMonsterBrush() :
	Brush() {
	////
}

SpawnMonsterBrush::~SpawnMonsterBrush() {
	////
}

int SpawnMonsterBrush::getLookID() const {
	return 0;
}

std::string SpawnMonsterBrush::getName() const {
	return "SpawnMonster Brush";
}

bool SpawnMonsterBrush::canDraw(BaseMap* map, const Position &position) const {
	Tile* tile = map->getTile(position);
	if (tile && tile->ground) {
		if (tile->spawnMonster) {
			return false;
		}
	}
	return true;
}

void SpawnMonsterBrush::undraw(BaseMap* map, Tile* tile) {
	delete tile->spawnMonster;
	tile->spawnMonster = nullptr;
}

void SpawnMonsterBrush::draw(BaseMap* map, Tile* tile, void* parameter) {
	ASSERT(tile);
	ASSERT(parameter); // Should contain an int which is the size of the newd monster spawn

	if (!tile->ground) {
		return;
	}

	auto size = std::max(1, *(int*)parameter);
	auto side = size * 2 + 1;
	uint16_t spawnTime = g_settings.getInteger(Config::DEFAULT_SPAWN_MONSTER_TIME);
	int density = g_settings.getInteger(Config::SPAWN_MONSTER_DENSITY);
	if (tile && tile->spawnMonster == nullptr) {
		tile->spawnMonster = newd SpawnMonster(size);
		auto toSpawn = (int)std::ceil((side * side) * (density / 100.0));
		std::set<Position> positions;
		for (int i = 0; i < side; i++) {
			for (int j = 0; j < side; j++) {
				positions.insert(Position(tile->getPosition().x - size + i, tile->getPosition().y - size + j, tile->getPosition().z));
			}
		}

		if (monsters.empty()) {
			return;
		}

		for (int i = 0; i < toSpawn; ++i) {
			Tile* tileSpawn = nullptr;

			auto iter = positions.begin();
			while (!positions.empty() && (!tileSpawn || !tileSpawn->ground)) {
				std::advance(iter, rand() % positions.size());
				tileSpawn = map->getTile(*iter);
				if (tileSpawn) {
					tileSpawn = tileSpawn->getPosition() == tile->getPosition() ? tile : tileSpawn;
				}
				positions.erase(iter);
				iter = positions.begin();
			}

			if (tileSpawn) {
				auto monsterBrush = monsters[rand() % monsters.size()];
				monsterBrush->drawMonster(map, tileSpawn, &spawnTime);
			}
		}
	}
}
