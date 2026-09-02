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
#include "MonsterActor.h"
#include "NavigationGrid.h"
#include "GameHUD.h"
#include "GameState.h"

class Actor;
class MeshComponent;
class GeneratorActor;

class LightSaverGame : public GameLoop
{
public:
	virtual bool OnInitialize() override;
	virtual void Update(float deltaTime) override;
	virtual bool Render() override;

	~LightSaverGame() override;
private:
	PlayerController MainPlayerController;
	PlayerActor* MainPlayer = nullptr;
	Model SpiderModel; 
	Model FloorModel; 
	Model WallModel;
	Model GeneratorModel;
	World GameWorld;
	MonsterActor* SpiderActor = nullptr;
	GeneratorActor* LightGenerator = nullptr;
	GameState CurrentGameState = GameState::Playing;
	Renderer RenderManager;
	NavigationGrid MonsterNavGrid;
	GameHUD HUD;
};
