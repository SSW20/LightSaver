#pragma once
#include "Model.h"
#include "Transform.h"
struct RenderObject
{
	Model* ModelSet = nullptr;
	Transform ModelWorldTransform;
};

