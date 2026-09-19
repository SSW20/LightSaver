#include "GeneratorActor.h"
#include <algorithm>
#include "SoundManager.h"
bool GeneratorActor::IsRepaired() const
{
    return bRepaired;
}

void GeneratorActor::Repairing(float DeltaTime)
{
    if (bRepaired) return;
    if (CurrentRepairTime < TotalRepairTime)
    {
        CurrentRepairTime += DeltaTime;
    }
    if (CurrentRepairTime >= TotalRepairTime)
    {
		CurrentRepairTime = TotalRepairTime;
        bRepaired = true;
		SoundManager::Get().StartLoop3D(
			SoundID::GeneratorLoop,
			GetActorTransform().Position,
			0.48f);
		SoundManager::Get().Play3D(
			SoundID::GeneratorComplete,
			GetActorTransform().Position,
			0.78f);
        return;
    }
}

float GeneratorActor::GetRepairProgress() const
{
    if (TotalRepairTime == 0.0f) return 0.0f;
    return std::min(1.0f, CurrentRepairTime / TotalRepairTime);
}

void GeneratorActor::Interact(float DeltaTime)
{
    Repairing(DeltaTime);
}

void GeneratorActor::Reset()
{
	SoundManager::Get().StopLoop(SoundID::GeneratorLoop);
	bRepaired = false;
	CurrentRepairTime = 0.0f;
}

void GeneratorActor::OnUpdate(float DeltaTime)
{
	(void)DeltaTime;
	if (bRepaired)
	{
		SoundManager::Get().UpdateLoop3D(
			SoundID::GeneratorLoop,
			GetActorTransform().Position);
	}
}
