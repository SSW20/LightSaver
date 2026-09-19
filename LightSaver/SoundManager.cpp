#include "SoundManager.h"

#include <algorithm>
#include <exception>
#include <utility>

SoundManager& SoundManager::Get()
{
	static SoundManager Instance;
	return Instance;
}

bool SoundManager::Initialize()
{
	if (Engine != nullptr)
	{
		return true;
	}

	try
	{
		Engine = std::make_unique<DirectX::AudioEngine>();
		Sounds.emplace(SoundID::Test, std::make_unique<DirectX::SoundEffect>(Engine.get(), L"Assets/Audio/Test.wav"));
		Sounds.emplace(SoundID::PlayerFootstep, std::make_unique<DirectX::SoundEffect>(Engine.get(), L"Assets/Audio/PlayerFootstep.wav"));
		Sounds.emplace(SoundID::PlayerHeartbeat, std::make_unique<DirectX::SoundEffect>(Engine.get(), L"Assets/Audio/PlayerHeartbeat.wav"));
		auto& BreathSounds = RandomSounds[SoundID::PlayerBreath];
		BreathSounds.emplace_back(std::make_unique<DirectX::SoundEffect>(Engine.get(), L"Assets/Audio/PlayerBreath1.wav"));
		BreathSounds.emplace_back(std::make_unique<DirectX::SoundEffect>(Engine.get(), L"Assets/Audio/PlayerBreath2.wav"));
		BreathSounds.emplace_back(std::make_unique<DirectX::SoundEffect>(Engine.get(), L"Assets/Audio/PlayerBreath3.wav"));
		Sounds.emplace(SoundID::PlayerDamage, std::make_unique<DirectX::SoundEffect>(Engine.get(), L"Assets/Audio/PlayerDamage.wav"));
		Sounds.emplace(SoundID::FlashlightOff, std::make_unique<DirectX::SoundEffect>(Engine.get(), L"Assets/Audio/FlashlightOff.wav"));
		Sounds.emplace(SoundID::GeneratorLoop, std::make_unique<DirectX::SoundEffect>(Engine.get(), L"Assets/Audio/GeneratorLoop.wav"));
		Sounds.emplace(SoundID::GeneratorComplete, std::make_unique<DirectX::SoundEffect>(Engine.get(), L"Assets/Audio/GeneratorComplete.wav"));
		Sounds.emplace(SoundID::MonsterFootstep, std::make_unique<DirectX::SoundEffect>(Engine.get(), L"Assets/Audio/MonsterFootstep.wav"));
		Sounds.emplace(SoundID::MonsterAttack, std::make_unique<DirectX::SoundEffect>(Engine.get(), L"Assets/Audio/MonsterAttack.wav"));
		Sounds.emplace(SoundID::GameOver, std::make_unique<DirectX::SoundEffect>(Engine.get(), L"Assets/Audio/GameOver.wav"));
		Sounds.emplace(SoundID::GameWin, std::make_unique<DirectX::SoundEffect>(Engine.get(), L"Assets/Audio/GameWin.wav"));
		Sounds.emplace(SoundID::UIClick, std::make_unique<DirectX::SoundEffect>(Engine.get(), L"Assets/Audio/UIClick.wav"));
		// Sounds.emplace(SoundID::BackgroundMusic, std::make_unique<DirectX::SoundEffect>(Engine.get(), L"Assets/Audio/BackgroundMusic.wav"));
	}
	catch (const std::exception&)
	{
		Shutdown();
		return false;
	}

	return true;
}

bool SoundManager::Update()
{
	if (Engine == nullptr)
	{
		return false;
	}

	for (auto& Pair : LoopingSounds)
	{
		SoundInstance* Sound = Pair.second.get();
		if (Sound != nullptr && Sound->bSpatial && Sound->Instance != nullptr)
		{
			Sound->Instance->Apply3D(Listener, Sound->Emitter, false);
		}
	}

	for (auto& Sound : ActiveSpatialSounds)
	{
		if (Sound != nullptr && Sound->Instance != nullptr)
		{
			Sound->Instance->Apply3D(Listener, Sound->Emitter, false);
		}
	}

	const bool bUpdated = Engine->Update();
	ActiveSpatialSounds.erase(
		std::remove_if(
			ActiveSpatialSounds.begin(),
			ActiveSpatialSounds.end(),
			[](const std::unique_ptr<SoundInstance>& Sound)
			{
				return Sound == nullptr || Sound->Instance == nullptr ||
					Sound->Instance->GetState() == DirectX::SoundState::STOPPED;
			}),
		ActiveSpatialSounds.end());

	return bUpdated;
}

void SoundManager::Play2D(SoundID ID, float Volume)
{
	DirectX::SoundEffect* Sound = FindSound(ID);
	if (Sound == nullptr)
	{
		return;
	}

	const float ClampedVolume = std::clamp(Volume, 0.0f, 1.0f);
	Sound->Play(ClampedVolume, 0.0f, 0.0f);
}

void SoundManager::PlayRandom2D(SoundID ID, float Volume)
{
	const auto FoundSounds = RandomSounds.find(ID);
	if (FoundSounds == RandomSounds.end() || FoundSounds->second.empty())
	{
		return;
	}

	const auto& Candidates = FoundSounds->second;
	std::uniform_int_distribution<size_t> Distribution(0, Candidates.size() - 1);
	size_t SelectedIndex = Distribution(RandomGenerator);

	const auto FoundLastIndex = LastRandomSoundIndices.find(ID);
	if (Candidates.size() > 1 && FoundLastIndex != LastRandomSoundIndices.end() &&
		SelectedIndex == FoundLastIndex->second)
	{
		SelectedIndex = (SelectedIndex + 1) % Candidates.size();
	}

	LastRandomSoundIndices[ID] = SelectedIndex;
	Candidates[SelectedIndex]->Play(std::clamp(Volume, 0.0f, 1.0f), 0.0f, 0.0f);
}

void SoundManager::Play3D(SoundID ID, const DirectX::XMFLOAT3& Position, float Volume)
{
	std::unique_ptr<SoundInstance> Sound = CreateInstance(ID, true, Volume, &Position);
	if (Sound == nullptr || Sound->Instance == nullptr)
	{
		return;
	}

	Sound->Instance->Play(false);
	ActiveSpatialSounds.push_back(std::move(Sound));
}

void SoundManager::StartLoop2D(SoundID ID, float Volume)
{
	auto FoundLoop = LoopingSounds.find(ID);
	if (FoundLoop != LoopingSounds.end())
	{
		if (FoundLoop->second != nullptr && FoundLoop->second->Instance != nullptr)
		{
			FoundLoop->second->Instance->SetVolume(std::clamp(Volume, 0.0f, 1.0f));
			if (FoundLoop->second->Instance->GetState() != DirectX::SoundState::PLAYING)
			{
				FoundLoop->second->Instance->Play(true);
			}
		}
		return;
	}

	std::unique_ptr<SoundInstance> Sound = CreateInstance(ID, false, Volume, nullptr);
	if (Sound == nullptr || Sound->Instance == nullptr)
	{
		return;
	}

	Sound->Instance->Play(true);
	LoopingSounds.emplace(ID, std::move(Sound));
}

void SoundManager::StartLoop3D(SoundID ID, const DirectX::XMFLOAT3& Position, float Volume)
{
	auto FoundLoop = LoopingSounds.find(ID);
	if (FoundLoop != LoopingSounds.end())
	{
		SoundInstance* Sound = FoundLoop->second.get();
		if (Sound != nullptr && Sound->Instance != nullptr)
		{
			Sound->Emitter.SetPosition(Position);
			Sound->Instance->SetVolume(std::clamp(Volume, 0.0f, 1.0f));
			Sound->Instance->Apply3D(Listener, Sound->Emitter, false);
			if (Sound->Instance->GetState() != DirectX::SoundState::PLAYING)
			{
				Sound->Instance->Play(true);
			}
		}
		return;
	}

	std::unique_ptr<SoundInstance> Sound = CreateInstance(ID, true, Volume, &Position);
	if (Sound == nullptr || Sound->Instance == nullptr)
	{
		return;
	}

	Sound->Instance->Play(true);
	LoopingSounds.emplace(ID, std::move(Sound));
}

void SoundManager::UpdateLoop3D(SoundID ID, const DirectX::XMFLOAT3& Position)
{
	const auto FoundLoop = LoopingSounds.find(ID);
	if (FoundLoop == LoopingSounds.end() || FoundLoop->second == nullptr ||
		!FoundLoop->second->bSpatial)
	{
		return;
	}

	FoundLoop->second->Emitter.SetPosition(Position);
}

void SoundManager::StopLoop(SoundID ID)
{
	const auto FoundLoop = LoopingSounds.find(ID);
	if (FoundLoop == LoopingSounds.end())
	{
		return;
	}

	if (FoundLoop->second != nullptr && FoundLoop->second->Instance != nullptr)
	{
		FoundLoop->second->Instance->Stop(true);
	}
	LoopingSounds.erase(FoundLoop);
}

void SoundManager::StopAllLoops()
{
	for (auto& Pair : LoopingSounds)
	{
		if (Pair.second != nullptr && Pair.second->Instance != nullptr)
		{
			Pair.second->Instance->Stop(true);
		}
	}
	LoopingSounds.clear();
}

void SoundManager::SetListener(
	DirectX::FXMVECTOR Position,
	DirectX::FXMVECTOR Forward,
	DirectX::FXMVECTOR Up)
{
	Listener.SetPosition(Position);
	Listener.SetOrientation(Forward, Up);
}

DirectX::SoundEffect* SoundManager::FindSound(SoundID ID) const
{
	// 손전등을 켜고 끌 때 하나의 실제 SoundEffect를 공유한다.
	if (ID == SoundID::FlashlightOn)
	{
		ID = SoundID::FlashlightOff;
	}

	const auto FoundSound = Sounds.find(ID);
	if (FoundSound == Sounds.end() || FoundSound->second == nullptr)
	{
		return nullptr;
	}
	return FoundSound->second.get();
}

std::unique_ptr<SoundManager::SoundInstance> SoundManager::CreateInstance(
	SoundID ID,
	bool bSpatial,
	float Volume,
	const DirectX::XMFLOAT3* Position)
{
	DirectX::SoundEffect* Sound = FindSound(ID);
	if (Sound == nullptr)
	{
		return nullptr;
	}

	auto NewSound = std::make_unique<SoundInstance>();
	NewSound->bSpatial = bSpatial;
	const DirectX::SOUND_EFFECT_INSTANCE_FLAGS Flags = bSpatial
		? DirectX::SoundEffectInstance_Use3D
		: DirectX::SoundEffectInstance_Default;
	NewSound->Instance = Sound->CreateInstance(Flags);
	NewSound->Instance->SetVolume(std::clamp(Volume, 0.0f, 1.0f));

	if (bSpatial && Position != nullptr)
	{
		NewSound->Emitter.SetPosition(*Position);
		// 병원 월드 좌표에서 수 m 떨어져도 급격히 무음이 되지 않게 감쇠 범위를 늘린다.
		NewSound->Emitter.CurveDistanceScaler = 20.0f;
		NewSound->Instance->Apply3D(Listener, NewSound->Emitter, false);
	}

	return NewSound;
}

void SoundManager::Shutdown()
{
	// 모든 SoundEffect와 SoundEffectInstance는 AudioEngine의 알림 목록에 등록되어 있다.
	// 따라서 객체부터 지우면 오디오 스레드가 이미 해제된 알림 객체를 호출할 수 있다.
	// 객체가 모두 살아 있는 동안 Engine을 먼저 종료하여 콜백과 Voice를 확실히 정리한다.
	if (Engine != nullptr)
	{
		Engine->Suspend();
		Engine.reset();
	}

	LoopingSounds.clear();
	ActiveSpatialSounds.clear();
	RandomSounds.clear();
	LastRandomSoundIndices.clear();
	Sounds.clear();
}

SoundManager::~SoundManager()
{
	Shutdown();
}
