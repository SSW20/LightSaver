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
#include "HospitalLevel.h"

class Actor;
class MeshComponent;
class GeneratorActor;
class ExitZoneActor;

class LightSaverGame : public GameLoop
{
public:
	virtual bool OnInitialize() override;
	virtual void Update(float deltaTime) override;
	virtual bool Render() override;

	~LightSaverGame() override;
private:
	void StartGame();

	PlayerController MainPlayerController;
	PlayerActor* MainPlayer = nullptr;
	Model SpiderModel; 
	Model GeneratorModel;
	HospitalLevel Hospital;
	World GameWorld;
	MonsterActor* SpiderActor = nullptr;
	GeneratorActor* LightGenerator = nullptr;
	ExitZoneActor* ExitZone = nullptr;
	GameState CurrentGameState = GameState::MainMenu;
	Renderer RenderManager;
	NavigationGrid FirstFloorMonsterNavGrid;
	NavigationGrid SecondFloorMonsterNavGrid;
	GameHUD HUD;
	DirectX::XMFLOAT3 PlayerSpawnPosition = {};
	DirectX::XMFLOAT3 SpiderSpawnPosition = {};
};
