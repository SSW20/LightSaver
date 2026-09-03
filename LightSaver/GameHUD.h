#pragma once
#include "UIRenderer.h"
#include "GameState.h"

class Graphics;
class InputManager;

enum class GameHUDAction
{
	None,
	StartGame,
	ExitGame
};

class GameHUD
{
public:
    bool Initialize(Graphics& InGraphics);
	GameHUDAction UpdateResultInput(GameState State, const InputManager& Input);
    bool Render(GameState State, float DamageAlpha, bool bFocusTarget, bool bInteracting, float RepairProgress);

private:
	struct ButtonBounds
	{
		float Left = 0.0f;
		float Top = 0.0f;
		float Right = 0.0f;
		float Bottom = 0.0f;

		bool Contains(float X, float Y) const;
	};

	void GetResultButtonBounds(ButtonBounds& OutStart, ButtonBounds& OutExit) const;

    UIRenderer Renderer;
    Texture BloodOverlayTexture;
    Texture YouDiedTexture;
    Texture EKeyTexture;
	Texture LightSaverTitleTexture;
	Texture PressEKeyToStartTexture;
	Texture EscapeTexture;
	Texture StartGameButtonTexture;
	Texture ExitGameButtonTexture;
	bool bStartButtonHovered = false;
	bool bExitButtonHovered = false;
};
