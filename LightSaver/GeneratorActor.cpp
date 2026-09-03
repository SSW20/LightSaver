#include "GeneratorActor.h"
#include <algorithm>
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
	bRepaired = false;
	CurrentRepairTime = 0.0f;
}
