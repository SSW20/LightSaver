#include "Actor.h"

void Actor::Update(float DeltaTime)
{
	for (int i=0; i<Components.size(); ++i)
	{
		if (Components[i] == nullptr) continue;
		Components[i]->Update(DeltaTime);
	}
}

Actor::~Actor() = default;