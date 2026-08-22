#include "World.h"
#include "Actor.h"

void World::AddActor(std::unique_ptr<Actor> Actor)
{
	if (Actor == nullptr) return;
	Actors.push_back(std::move(Actor));
}

void World::Update(float DeltaTime)
{
	for (int i = 0; i < Actors.size(); ++i)
	{
		if (Actors[i] == nullptr) continue;
		Actors[i]->Update(DeltaTime);
	}
}
World::World() = default;
World::~World() = default;

