#pragma once
#include <vector>
#include <memory>
#include "Actor.h"


class World
{
public:
	void AddActor(std::unique_ptr<Actor> Actor);
	void Update(float DeltaTime);
	const std::vector<std::unique_ptr<Actor>>& GetActors() const { return Actors; }
	World();
	~World();
	template<typename ActorType>
	inline ActorType* SpawnActor();

	void CollectRenderObjects(std::vector<RenderObject>& OutRenderObjects) const;
private:
	std::vector<std::unique_ptr<Actor>> Actors;
};

template<typename ActorType>
inline ActorType* World::SpawnActor()
{
	auto NewActor = std::make_unique<ActorType>();
	ActorType* ActorAddr = NewActor.get();
	Actors.push_back(std::move(NewActor));

	return ActorAddr;
}

