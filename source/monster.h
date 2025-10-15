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

#ifndef RME_MONSTER_H_
#define RME_MONSTER_H_

#include "monsters.h"
#include "enums.h"

class Monster {
public:
	Monster(MonsterType* type, uint8_t weight = 0);
	Monster(const std::string &type_name, uint8_t weight = 0);

	Monster* deepCopy() const;

	const Outfit &getLookType() const;

	bool isSaved() const noexcept {
		return saved;
	}
	void save() noexcept {
		saved = true;
	}
	void reset() noexcept {
		saved = false;
	}

	bool isSelected() const noexcept {
		return selected;
	}
	void deselect() noexcept {
		selected = false;
	}
	void select() noexcept {
		selected = true;
	}

	[[nodiscard]] const std::string &getTypeName() const noexcept {
		return type_name;
	}

	std::string getName() const;
	MonsterBrush* getBrush() const;

	[[nodiscard]] int getWeight() const noexcept {
		return weight;
	}

	void setWeight(int newWeight) noexcept {
		weight = newWeight;
	}

	uint16_t getSpawnMonsterTime() const noexcept {
		return spawntime;
	}
	void setSpawnMonsterTime(uint16_t time) noexcept {
		spawntime = time;
	}

	Direction getDirection() const noexcept {
		return direction;
	}
	void setDirection(Direction _direction) noexcept {
		direction = _direction;
	}

	// Static conversions
	static std::string DirID2Name(uint16_t id);
	static uint16_t DirName2ID(std::string id);

protected:
	std::string type_name;
	Direction direction;
	uint8_t weight;
	uint16_t spawntime;
	bool saved;
	bool selected;
};

typedef std::vector<Monster*> MonsterVector;
typedef std::list<Monster*> MonsterList;

#endif
