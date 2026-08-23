#pragma once
#include "Component.h"
#include "RenderObject.h"
#include "Actor.h"
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
	Model* GetModel() const { return ModelSet; }
	RenderObject CreateRenderObj() const;
	virtual void CollectRenderObjects(std::vector<RenderObject>& RenderObjects) const override;
private:
	Model* ModelSet = nullptr;
};

