#pragma once
#include <vector>
#include <memory>
#include "Actor.h"


class World
{
public:
	void AddActor(std::unique_ptr<Actor> Actor);
	void Update(float DeltaTime);
	World();
	~World();
	template<typename ActorType>
	inline ActorType* SpawnActor();

private:
	std::vector<std::unique_ptr<Actor>> Actors;
};

template<typename ActorType>
inline ActorType* World::SpawnActor()
{
	auto NewActor = std::make_unique<ActorType>();
	Actor* ActorAddr = NewActor.get();
	Actors.push_back(std::move(NewActor));

	return ActorAddr;
}

