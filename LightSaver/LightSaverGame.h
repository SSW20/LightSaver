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
	float CameraSpeed = 3.0f;
	// 픽셀당 회전할 라디안 값
	float MouseSpeed = DirectX::XMConvertToRadians(0.1f);
	Camera MainCamera;
	Model SpiderModel; 
	Model FloorModel; 
	Model WallModel;
	World GameWorld;
	Actor* SpiderActor;
	Renderer RenderManager;
	float VerticalVelocity = 0.0f;
	float Gravity = -9.8f;
	float JumpSpeed = 5.0f;
};
