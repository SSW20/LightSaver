#pragma once
#include "GameLoop.h"
#include <dxgi.h>
#include <DirectXMath.h>
#include "Camera.h"
#include "Shader.h"
#include "Mesh.h"
#include "Texture.h"
#include "Model.h"
#include "Transform.h"
#include "World.h"
#include "Renderer.h"
#include "BoxColliderComponent.h"
#include "PlayerController.h"

class Actor;
class MeshComponent;

class LightSaverGame : public GameLoop
{
public:
	virtual bool OnInitialize() override;
	virtual void Update(float deltaTime) override;
	virtual bool Render() override;

	~LightSaverGame() override;


private:
	float Rotation = 0.0f;
	PlayerController MainPlayerController;
	PlayerActor* MainPlayer = nullptr;
	Model SpiderModel; 
	Model FloorModel; 
	Model WallModel;
	World GameWorld;
	Actor* SpiderActor;
	Renderer RenderManager;
};
