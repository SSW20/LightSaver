#include "GameHUD.h"

bool GameHUD::Initialize(Graphics& InGraphics)
{
	YouDiedTexture.Initialize(InGraphics.Device, L"Assets/UI/YouDead.png");
	BloodOverlayTexture.Initialize(InGraphics.Device, L"Assets/UI/BloodBorder.png");
	EKeyTexture.Initialize(InGraphics.Device, L"Assets/UI/EKey.png");
	return Renderer.Initialize(InGraphics);
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
		float ImageWidth = 600.0f;
		float ImageHeight = 600.0f;
		float ImageTop = 80.0f;
		Renderer.AddRectanglePixelsImage(CenterX - ImageWidth * 0.5f, ImageTop, CenterX + ImageWidth * 0.5f, ImageTop + ImageHeight, White, &YouDiedTexture);
		break;
	}

	case GameState::GameClear:
		Renderer.AddRectanglePixels(0.0f, 0.0f, Renderer.GetGraphics()->Width, Renderer.GetGraphics()->Height, { 0.0f, 0.35f, 0.1f, 1.0f });
		break;
	}

	return Renderer.EndFrame();
}
