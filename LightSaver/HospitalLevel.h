#pragma once

#include "Model.h"

class World;
struct AABB;

class HospitalLevel
{
public:
	bool Initialize(ID3D11Device* Device, World& GameWorld);

private:
	void BuildFloor(World& GameWorld, float FloorHeight, bool bSecondFloor);
	void SpawnStairs(World& GameWorld);
	void SpawnFloor(World& GameWorld, float X, float Y, float Z);
	void SpawnCeiling(World& GameWorld, float X, float Y, float Z);
	void SpawnWall(World& GameWorld, float X, float Y, float Z, float Yaw);
	void SpawnDoorway(World& GameWorld, float X, float Y, float Z, float Yaw);
	void SpawnCollisionBox(World& GameWorld, float X, float Y, float Z, float Yaw, const AABB& CollisionBox);
	void SpawnStairStep(World& GameWorld, float X, float Y, float Z, float Height);
	void SpawnStairRampCollider(World& GameWorld);
	void SpawnExitSign(World& GameWorld);

	Model FloorModel;
	Model WallModel;
	Model DoorwayModel;
	Model CeilingModel;
	Model ExitSignModel;
};
