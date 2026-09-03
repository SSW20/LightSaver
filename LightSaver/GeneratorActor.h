#pragma once
#include "Actor.h"
#include "Interactable.h"

class GeneratorActor : public Actor, public Interactable
{
public:
	bool IsRepaired() const;
	void Repairing(float DeltaTime);
	float GetRepairProgress() const;
	void Interact(float DeltaTime) override;
	void Reset();

private:
	bool bRepaired = false;
	float TotalRepairTime = 3.0f;
	float CurrentRepairTime = 0.0f;
};

