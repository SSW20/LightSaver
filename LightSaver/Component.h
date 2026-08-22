#pragma once

class Actor;
class Component
{
public:
	Component(Actor* Owner)
		:pOwner(Owner) {};

	virtual ~Component() = default;

	virtual void Update(float DeltaTime);
	Actor* GetOwner() const { return pOwner; }

private:
	Actor* pOwner = nullptr;
};

