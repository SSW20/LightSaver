#pragma once
#include "Transform.h"
#include <memory>
#include <vector>
#include "Component.h"

class Actor
{
public:
	const Transform& GetActorTransform() const { return ActorTransform; }
	Transform& GetActorTransform()  { return ActorTransform; }
	void Update(float Deltatime);

	template<typename ComponentType, typename... Args>
	ComponentType* AddComponent(Args&& ...Arguments);

	template<typename ComponentType>
	ComponentType* FindComponent();


	const std::vector<std::unique_ptr<Component>>& GetComponents() const { return Components; }
	void CollectRenderObjects(std::vector<RenderObject>& OutRenderObjects) const;

	virtual ~Actor();
protected:
	virtual void OnUpdate(float DeltaTime);
private:
	Transform ActorTransform;
	std::vector<std::unique_ptr<Component>> Components;
};

template<typename ComponentType, typename ...Args>
inline ComponentType* Actor::AddComponent(Args && ...Arguments)
{	
	auto NewComponent = std::make_unique<ComponentType>(this, Arguments ...);

	ComponentType* ComponentAddr = NewComponent.get();
	Components.push_back(std::move(NewComponent));
	return ComponentAddr;
}

template<typename ComponentType>
inline ComponentType* Actor::FindComponent()
{
	for (const auto& Comp : Components)
	{
		ComponentType* FindComp = dynamic_cast<ComponentType*>(Comp.get());
		if (FindComp != nullptr)
		{
			return FindComp;
		}
		else continue;
	}
	return nullptr;
}
