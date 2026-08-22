#pragma once
#include "Component.h"

class Model;
class MeshComponent : public Component
{
public:
	MeshComponent(Actor* Owner, Model* NewModel)
		:Component(Owner), ModelSet(NewModel) {
	};

	void SetModel(Model* NewModel)
	{
		if (NewModel == nullptr) return;
		ModelSet = NewModel;
	}
	Model* GetModel() const
	{
		return ModelSet;
	}
private:
	Model* ModelSet = nullptr;
};

