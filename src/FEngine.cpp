#include "FEngine.hpp"

FProjectile _createProjectile(FProjectileType type, const FVector& position, const FVector& velocity)
{
	if (type == FProjectileType::DEBUG)
	{
		auto size = FVector(10, 10);
		auto projectile = FProjectile(position, size, type, 1000.f, false, false);
		projectile.setVelocity(velocity);
		return projectile;
	}
	
	return FProjectile(FVector(), FVector(), FProjectileType::DEBUG, 0.f, false, false);
}

FPowerup _createPowerup(FPowerupType type, const FVector& position)
{
	if (type == FPowerupType::DEBUG)
	{
		auto size = FVector(25.f, 25.f);
		return FPowerup(position, size, type, 10.f);
	}
	
	return FPowerup(FVector(), FVector(), FPowerupType::DEBUG, 0.f);
}

FEffect _createEffect(FEffectType type, const FVector& center)
{
	if (type == FEffectType::DEBUG)
	{
		auto size = FVector(96.f, 96.f);
		auto position = FVector(center.x - size.x / 2.f, center.y - size.y / 2.f);
		return FEffect(position, size, type, 0.5f);
	}
	
	return FEffect(FVector(), FVector(), FEffectType::DEBUG, 0.f);
}

// --- --- --- --- --- --- --- --- --- --- --- --- --- --- --- 

FEngine::FEngine(std::shared_ptr<FLevel> level)
{
	m_level = level;
	m_ticks = 0;
}

void FEngine::addPlayer(FCharacter player)
{
	m_characters.push_back(player);
}

std::shared_ptr<FLevel> FEngine::getLevel()
{
	return m_level;
}
	
void FEngine::tick(float delta)
{	
	for (auto &character : m_characters)
	{
		character.tick(delta);
		character.updateAI(&m_characters, &m_projectiles, &m_powerups);
		
		if (character.getHealth() > 0)
		{
			auto &weapon = character.getWeapon();
			if (weapon.hasFired())
			{
				FVector position, velocity;
				if (character.facingLeft())
				{
					position = FVector(-character.getSize().x * 0.5f, character.getSize().y / 2.f) + character.getPosition();
					velocity = FVector(character.getVelocity().x - 1.f, 0.f);
				}
				else
				{
					position = FVector(character.getSize().x * 1.5f, character.getSize().y / 2.f) + character.getPosition();
					velocity = FVector(character.getVelocity().x + 1.f, 0.f);
				}				
				createProjectile(position, velocity, weapon.getProjectileType());
			}
		}
	}
	
	for (auto &projectile : m_projectiles)
	{
		projectile.tick(delta);
	}
	
	for (auto &powerup : m_powerups)
	{
		powerup.tick(delta);
	}
	
	for (auto &effect : m_effects)
	{
		effect.tick(delta);
	}

	collisionDetection();
	
	for (auto &character : m_characters)
	{
		if (character.getHealth() <= 0)
		{
			// TODO: This only has to happen once per death
			character.setPosition(findEmptySpace(character.getSize()));
		}
	}
	
	std::random_device rd;
	std::mt19937 gen(rd());
	std::uniform_real_distribution<> randomvalue(0, 1);
	if (randomvalue(gen) < CREATING_POWERUP_PROBABILITY)
		createPowerup();
	
	clean();	
	++m_ticks;
	m_time += delta;
}

bool FEngine::isRunning()
{
	std::vector<int> teams;
	for (auto character : m_characters)
		if (character.getLives() > 0)
			teams.push_back(character.getTeam());
	for (auto first = teams.begin(); first != teams.end() - 1; ++first)
		for (auto second = first + 1; second != teams.end(); ++second)
			if (*first != *second)
				return true;
	return false;
}

void FEngine::print()
{
	//TODO
}

int FEngine::getTickCount()
{
	return m_ticks;
}

float FEngine::getTime()
{
	return m_time;
}

void FEngine::createProjectile(const FVector& position, const FVector& velocity, FProjectileType type)
{
	m_projectiles.push_back(_createProjectile(type, position, velocity));
}

void FEngine::createPowerup()
{
	FPowerupType type = FPowerupType::DEBUG;
	auto space = FVector(50.f, 50.f);
	FVector position = findEmptySpace(space);
	m_powerups.push_back(_createPowerup(type, position));
}

void FEngine::createEffect(const FVector& position, FEffectType type)
{
	m_effects.push_back(_createEffect(type, position));
}

void FEngine::clean()
{
	for (auto it = m_powerups.begin(); m_powerups.size() > 0 && it != m_powerups.end();)
	{
		if (!it->isAlive())
		{
			it = m_powerups.erase(it);
		}
		else
		{
			++it;
		}
	}
	
	for (auto it = m_effects.begin(); m_effects.size() > 0 && it != m_effects.end();)
	{
		if (!it->isAlive())
		{
			it = m_effects.erase(it);
		}
		else
		{
			++it;
		}
	}
}

void FEngine::collisionDetection()
{
	auto tileSize = m_level->getTileSize();
	
	// Character&Wall
	for (auto &character : m_characters)
	{
		auto position = character.getPosition(), size = character.getSize();
		if (position.clampTo(0.f, -10.f, m_level->getWidth() * tileSize - size.x, m_level->getHeight() * tileSize))
		 	character.halt();
		
		if (position.clampTo(0.f, 0.f, m_level->getWidth() * tileSize - size.x, m_level->getHeight() * tileSize - size.y))
		 	character.land();

		character.m_standing = false;
		character.setPosition(position);
		
		if (collisionUp(character))
			character.land();
			
		if (collisionDown(character))
		{
			character.land();
			character.m_standing = true;
		}
			
		if (collisionLeft(character) || collisionRight(character))
			character.halt();
			
		character.m_standing = character.m_standing || position.y >= m_level->getHeight() * tileSize - size.y;
		character.m_ladder = touchingLadder(character);
	}
	
	// Projectile&Character
	for (auto it = m_projectiles.begin(); m_projectiles.size() > 0 && it != m_projectiles.end();)
	{
		bool erased = false;
		for (auto &character : m_characters)
		{
			if (character.getHealth() > 0 && it->intersects(character))
			{
				createEffect(it->getPosition(), FEffectType::DEBUG);				
				it = m_projectiles.erase(it);
				character.hurt(10);
				erased = true;
				break;
			}
		}
		if (!erased)
			++it;
	}
	
	// Projectile&Wall
	for (auto it = m_projectiles.begin(); m_projectiles.size() > 0 && it != m_projectiles.end();)
	{
		auto projectile = *it;
		auto position = projectile.getPosition(), size = projectile.getSize();
		bool outside = position.clampTo(0.f, 0.f, m_level->getWidth() * tileSize - size.x, m_level->getHeight() * tileSize - size.y);

		if (outside || collisionLeft(projectile) || collisionRight(projectile) || collisionUp(projectile) || collisionDown(projectile))
		{
			createEffect(it->getPosition(), FEffectType::DEBUG);
			it = m_projectiles.erase(it);
		}
		else
			++it;
	}
	
	// Powerup&Character
	for (auto it = m_powerups.begin(); m_powerups.size() > 0 && it != m_powerups.end();)
	{
		bool erased = false;
		for (auto &character : m_characters)
		{
			if (character.getHealth() > 0 && it->intersects(character))
			{
				character.heal(100);			
				it = m_powerups.erase(it);
				erased = true;
			}
		}
		if (!erased)
			++it;
	}
	
	// Powerup&Wall
	for (auto &powerup : m_powerups)
	{
		auto position = powerup.getPosition(), size = powerup.getSize();
		if (position.clampTo(0.f, 0.f, m_level->getWidth() * tileSize - size.x, m_level->getHeight() * tileSize - size.y))
		 	powerup.land();

		powerup.setPosition(position);
		
		if (collisionLeft(powerup) || collisionRight(powerup))
			powerup.halt();
			
		if (collisionUp(powerup) || collisionDown(powerup))
			powerup.land();
	}
}

bool FEngine::collisionUp(FObject& object)
{
	auto tileSize = m_level->getTileSize();
	auto position = object.getPosition(), size = object.getSize(); 
	for (float x = position.x; x <= position.x + size.x; x += tileSize)
	{
		if (tileIsSolid(m_level->get(x / tileSize, position.y / tileSize)))
		{
			position.y += (tileSize - fmod(position.y, tileSize));
			object.setPosition(position);
			return true;
		}
	}
	return false;
}

bool FEngine::collisionDown(FObject& object)
{
	auto tileSize = m_level->getTileSize();
	auto position = object.getPosition(), size = object.getSize(); 
	for (float x = position.x; x <= position.x + size.x; x += tileSize)
	{
		if (tileIsSolid(m_level->get(x / tileSize, (position.y + size.y) / tileSize)))
		{
			position.y -= fmod(position.y, tileSize) + 0.1f;
			object.setPosition(position);
			return true;
		}
	}
	return false;
}

bool FEngine::collisionLeft(FObject& object)
{
	auto tileSize = m_level->getTileSize();
	auto position = object.getPosition(), size = object.getSize();
	for (float y = position.y; y < position.y + size.y; y += tileSize)
	{
		if (tileIsSolid(m_level->get(position.x / tileSize, y / tileSize)))
		{
			position.x += (tileSize - fmod(position.x, tileSize));
			object.setPosition(position);
			return true;
		}
	}
	return false;
}

bool FEngine::collisionRight(FObject& object)
{
	auto tileSize = m_level->getTileSize();
	auto position = object.getPosition(), size = object.getSize();
	for (float y = position.y; y < position.y + size.y; y += tileSize)
	{
		if (tileIsSolid(m_level->get((position.x + size.x) / tileSize, y / tileSize)))
		{
			position.x -= fmod(position.x, tileSize);
			object.setPosition(position);
			return true;
		}
	}
	return false;
}

bool FEngine::touchingLadder(FObject& object)
{
	auto tileSize = m_level->getTileSize();
	auto position = object.getPosition(), size = object.getSize(); 
	for (float x = position.x; x <= position.x + size.x; x += tileSize)
	{
		for (float y = position.y; y <= position.y + size.y; y += tileSize)
		{
			if (tileIsLadder(m_level->get(x / tileSize, y / tileSize)))
			{
				return true;
			}
		}
	}
	return false;
}

std::vector<FCharacter>& FEngine::getCharacters()
{
	return m_characters;
}

std::vector<FProjectile>& FEngine::getProjectiles()
{
	return m_projectiles;
}

std::vector<FPowerup>& FEngine::getPowerups()
{
	return m_powerups;
}

std::vector<FEffect>& FEngine::getEffects()
{
	return m_effects;
}

FVector FEngine::findEmptySpace(FVector& size)
{
	auto tileSize = m_level->getTileSize();
	std::random_device rd;
	std::mt19937 gen(rd());
	std::uniform_real_distribution<> x(0, m_level->getWidth() * tileSize - size.x);
	std::uniform_real_distribution<> y(0, m_level->getHeight() * tileSize - size.y);
	bool result;
	
	while (true)
	{
		FVector position(x(gen), y(gen));
		result = true;
		
		for (float x = position.x; result && x <= position.x + size.x; x += tileSize)
		{
			for (float y = position.y; result && y <= position.y + size.y; y += tileSize)
			{
				if (tileIsSolid(m_level->get(x / tileSize, y / tileSize)))
					result = false;
			}
		}
		
		if (result)
			return position;
	}
	return FVector();
}
