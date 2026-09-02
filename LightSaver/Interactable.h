#pragma once
class Interactable
{
public:
	virtual void Interact(float DeltaTime) = 0;
	virtual ~Interactable() = default;
};

