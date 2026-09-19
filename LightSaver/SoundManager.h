#pragma once

#include <directxtk/Audio.h>
#include <DirectXMath.h>
#include <memory>
#include <random>
#include <unordered_map>
#include <vector>

enum class SoundID
{
	Test,

	PlayerFootstep,
	PlayerHeartbeat,
	PlayerBreath,
	PlayerDamage,
	FlashlightOn,
	FlashlightOff,

	GeneratorLoop,
	GeneratorComplete,

	MonsterFootstep,
	MonsterAttack,

	GameOver,
	GameWin,
	UIClick,
	BackgroundMusic
};

class SoundManager
{
public:
	static SoundManager& Get();

	bool Initialize();
	bool Update();
	void Shutdown();
	void Play2D(SoundID ID, float Volume = 1.0f);
	void PlayRandom2D(SoundID ID, float Volume = 1.0f);
	void Play3D(SoundID ID, const DirectX::XMFLOAT3& Position, float Volume = 1.0f);
	void StartLoop2D(SoundID ID, float Volume = 1.0f);
	void StartLoop3D(SoundID ID, const DirectX::XMFLOAT3& Position, float Volume = 1.0f);
	void UpdateLoop3D(SoundID ID, const DirectX::XMFLOAT3& Position);
	void StopLoop(SoundID ID);
	void StopAllLoops();
	void SetListener(
		DirectX::FXMVECTOR Position,
		DirectX::FXMVECTOR Forward,
		DirectX::FXMVECTOR Up);

private:
	struct SoundInstance
	{
		std::unique_ptr<DirectX::SoundEffectInstance> Instance;
		DirectX::AudioEmitter Emitter;
		bool bSpatial = false;
	};

	SoundManager() = default;
	~SoundManager();

	SoundManager(const SoundManager&) = delete;
	SoundManager& operator=(const SoundManager&) = delete;

	DirectX::SoundEffect* FindSound(SoundID ID) const;
	std::unique_ptr<SoundInstance> CreateInstance(
		SoundID ID,
		bool bSpatial,
		float Volume,
		const DirectX::XMFLOAT3* Position);

	std::unique_ptr<DirectX::AudioEngine> Engine;
	std::unordered_map<SoundID, std::unique_ptr<DirectX::SoundEffect>> Sounds;
	std::unordered_map<SoundID, std::vector<std::unique_ptr<DirectX::SoundEffect>>> RandomSounds;
	std::unordered_map<SoundID, size_t> LastRandomSoundIndices;
	std::unordered_map<SoundID, std::unique_ptr<SoundInstance>> LoopingSounds;
	std::vector<std::unique_ptr<SoundInstance>> ActiveSpatialSounds;
	std::mt19937 RandomGenerator{ std::random_device{}() };
	DirectX::AudioListener Listener;
};
