#pragma once
#include "UIRenderer.h"
#include "GameState.h"

class Graphics;

class GameHUD
{
public:
    bool Initialize(Graphics& InGraphics);
    bool Render(GameState State, float DamageAlpha, bool bFocusTarget, bool bInteracting, float RepairProgress);

private:
    UIRenderer Renderer;
    Texture BloodOverlayTexture;
    Texture YouDiedTexture;
    Texture EKeyTexture;
};
