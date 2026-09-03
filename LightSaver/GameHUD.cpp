#include "GameHUD.h"
#include "InputManager.h"

bool GameHUD::Initialize(Graphics& InGraphics)
{
	YouDiedTexture.Initialize(InGraphics.Device, L"Assets/UI/YouDead.png");
	BloodOverlayTexture.Initialize(InGraphics.Device, L"Assets/UI/BloodBorder.png");
	EKeyTexture.Initialize(InGraphics.Device, L"Assets/UI/EKey.png");
	LightSaverTitleTexture.Initialize(InGraphics.Device, L"Assets/UI/LightSaverTitle.png");
	PressEKeyToStartTexture.Initialize(InGraphics.Device, L"Assets/UI/PressEKeyToStart.png");
	EscapeTexture.Initialize(InGraphics.Device, L"Assets/UI/Escape.png");
	StartGameButtonTexture.Initialize(InGraphics.Device, L"Assets/UI/StartGameButton.png");
	ExitGameButtonTexture.Initialize(InGraphics.Device, L"Assets/UI/ExitGameButton.png");
	return Renderer.Initialize(InGraphics);
}

bool GameHUD::ButtonBounds::Contains(float X, float Y) const
{
	return X >= Left && X <= Right && Y >= Top && Y <= Bottom;
}

void GameHUD::GetResultButtonBounds(ButtonBounds& OutStart, ButtonBounds& OutExit) const
{
	const float ScreenWidth = Renderer.GetGraphics()->Width;
	const float ScreenHeight = Renderer.GetGraphics()->Height;
	const float ButtonWidth = ScreenWidth * 0.28f;
	const float ButtonHeight = ButtonWidth * (793.0f / 1983.0f);
	const float SideMargin = ScreenWidth * 0.08f;
	const float BottomMargin = ScreenHeight * 0.04f;
	const float ButtonBottom = ScreenHeight - BottomMargin;
	const float ButtonTop = ButtonBottom - ButtonHeight;

	OutStart = { SideMargin, ButtonTop, SideMargin + ButtonWidth, ButtonBottom };
	OutExit = { ScreenWidth - SideMargin - ButtonWidth, ButtonTop, ScreenWidth - SideMargin, ButtonBottom };
}

GameHUDAction GameHUD::UpdateResultInput(GameState State, const InputManager& Input)
{
	bStartButtonHovered = false;
	bExitButtonHovered = false;

	if (State != GameState::PlayerDead && State != GameState::GameClear)
	{
		return GameHUDAction::None;
	}

	ButtonBounds StartBounds;
	ButtonBounds ExitBounds;
	GetResultButtonBounds(StartBounds, ExitBounds);

	const POINT MousePosition = Input.GetMouseClientPosition();
	bStartButtonHovered = StartBounds.Contains(static_cast<float>(MousePosition.x), static_cast<float>(MousePosition.y));
	bExitButtonHovered = ExitBounds.Contains(static_cast<float>(MousePosition.x), static_cast<float>(MousePosition.y));

	if (!Input.IsKeyPressed(VK_LBUTTON))
	{
		return GameHUDAction::None;
	}

	if (bStartButtonHovered)
	{
		return GameHUDAction::StartGame;
	}

	if (bExitButtonHovered)
	{
		return GameHUDAction::ExitGame;
	}

	return GameHUDAction::None;
}

bool GameHUD::Render(GameState State, float DamageAlpha, bool bFocusTarget, bool bInteracting, float RepairProgress)
{
	Renderer.BeginFrame();

	const DirectX::XMFLOAT4 White = { 1.0f, 1.0f, 1.0f, 1.0f };

	float ScreenWidth = Renderer.GetGraphics()->Width;
	float ScreenHeight = Renderer.GetGraphics()->Height;

	float CenterX = ScreenWidth / 2.0f;
	float CenterY = ScreenHeight / 2.0f;

	// CrossHair
	float CrossHairHalfX = 12.0f;
	float CrossHairHalfY = 2.0f;
	if (DamageAlpha > 0.0f)
	{
		Renderer.AddRectanglePixelsImage(0.0f, 0.0f, ScreenWidth, ScreenHeight, { 1.0f, 1.0f, 1.0f, DamageAlpha }, &BloodOverlayTexture);
	}
	
	switch (State)
	{
	case GameState::MainMenu:
	{
		const float TitleWidth = ScreenWidth * 0.75f;
		const float TitleHeight = TitleWidth * (380.0f / 1632.0f);
		const float TitleTop = ScreenHeight * 0.12f;

		Renderer.AddRectanglePixelsImage(
			CenterX - TitleWidth * 0.5f,
			TitleTop,
			CenterX + TitleWidth * 0.5f,
			TitleTop + TitleHeight,
			White,
			&LightSaverTitleTexture);

		const float PromptWidth = ScreenWidth * 0.48f;
		const float PromptHeight = PromptWidth * (240.0f / 1492.0f);
		const float PromptTop = ScreenHeight * 0.75f;

		Renderer.AddRectanglePixelsImage(
			CenterX - PromptWidth * 0.5f,
			PromptTop,
			CenterX + PromptWidth * 0.5f,
			PromptTop + PromptHeight,
			White,
			&PressEKeyToStartTexture);

		break;
	}

	case GameState::Playing:
	{
		Renderer.AddRectanglePixels(CenterX - CrossHairHalfX, CenterY - CrossHairHalfY, CenterX + CrossHairHalfX, CenterY + CrossHairHalfY, White);
		Renderer.AddRectanglePixels(CenterX - CrossHairHalfY, CenterY - CrossHairHalfX, CenterX + CrossHairHalfY, CenterY + CrossHairHalfX, White);
		if (bFocusTarget)
		{
			float ImageWidth = 96.0f;
			float ImageHeight = 96.0f;
			float ImageTop = CenterY + 200.0f;
			Renderer.AddRectanglePixelsImage(CenterX - ImageWidth * 0.5f, ImageTop, CenterX + ImageWidth * 0.5f, ImageTop + ImageHeight, White, &EKeyTexture);
		}
		if (bInteracting)
		{
			float BarWidth = 800.0f;
			float BarHeight = 30.0f;
			float Padding = 4.0f;

			float BarLeft = CenterX - BarWidth * 0.5f;
			float BarRight = CenterX + BarWidth * 0.5f;
			float BarTop = ScreenHeight - 100.0f;
			float BarBottom = BarTop + BarHeight;

			// 검은색 배경
			Renderer.AddRectanglePixels(BarLeft, BarTop, BarRight, BarBottom, { 0.0f, 0.0f, 0.0f, 0.75f });

			float FillLeft = BarLeft + Padding;
			float FillTop = BarTop + Padding;
			float FillBottom = BarBottom - Padding;
			float InnerWidth = BarWidth - Padding * 2.0f;
			float FillRight = FillLeft + InnerWidth * RepairProgress;

			// 실제 진행 영역
			if (RepairProgress > 0.0f)
			{
				Renderer.AddRectanglePixels(FillLeft, FillTop, FillRight, FillBottom, { 1.0f, 0.75f, 0.1f, 1.0f });
			}
		}
		break;
	}

	case GameState::PlayerDead:
	{
		float ImageWidth = 500.0f;
		float ImageHeight = 500.0f;
		float ImageTop = 20.0f;
		Renderer.AddRectanglePixelsImage(CenterX - ImageWidth * 0.5f, ImageTop, CenterX + ImageWidth * 0.5f, ImageTop + ImageHeight, White, &YouDiedTexture);

		ButtonBounds StartBounds;
		ButtonBounds ExitBounds;
		GetResultButtonBounds(StartBounds, ExitBounds);
		const DirectX::XMFLOAT4 StartColor = bStartButtonHovered ? DirectX::XMFLOAT4{ 1.0f, 0.75f, 0.75f, 1.0f } : White;
		const DirectX::XMFLOAT4 ExitColor = bExitButtonHovered ? DirectX::XMFLOAT4{ 1.0f, 0.75f, 0.75f, 1.0f } : White;
		Renderer.AddRectanglePixelsImage(StartBounds.Left, StartBounds.Top, StartBounds.Right, StartBounds.Bottom, StartColor, &StartGameButtonTexture);
		Renderer.AddRectanglePixelsImage(ExitBounds.Left, ExitBounds.Top, ExitBounds.Right, ExitBounds.Bottom, ExitColor, &ExitGameButtonTexture);
		break;
	}

	case GameState::GameClear:
	{
		const float EscapeWidth = ScreenWidth * 0.58f;
		const float EscapeHeight = EscapeWidth * (724.0f / 2172.0f);
		const float EscapeTop = ScreenHeight * 0.18f;
		Renderer.AddRectanglePixelsImage(CenterX - EscapeWidth * 0.5f, EscapeTop, CenterX + EscapeWidth * 0.5f, EscapeTop + EscapeHeight, White, &EscapeTexture);

		ButtonBounds StartBounds;
		ButtonBounds ExitBounds;
		GetResultButtonBounds(StartBounds, ExitBounds);
		const DirectX::XMFLOAT4 StartColor = bStartButtonHovered ? DirectX::XMFLOAT4{ 1.0f, 0.75f, 0.75f, 1.0f } : White;
		const DirectX::XMFLOAT4 ExitColor = bExitButtonHovered ? DirectX::XMFLOAT4{ 1.0f, 0.75f, 0.75f, 1.0f } : White;
		Renderer.AddRectanglePixelsImage(StartBounds.Left, StartBounds.Top, StartBounds.Right, StartBounds.Bottom, StartColor, &StartGameButtonTexture);
		Renderer.AddRectanglePixelsImage(ExitBounds.Left, ExitBounds.Top, ExitBounds.Right, ExitBounds.Bottom, ExitColor, &ExitGameButtonTexture);
		break;
	}
	}

	return Renderer.EndFrame();
}
