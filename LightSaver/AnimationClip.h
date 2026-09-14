#pragma once
#include <string>
#include <vector>
#include <DirectXMath.h>
#include <unordered_map>
using namespace DirectX;

struct AnimationVectorKey
{
	XMFLOAT3 Value;
	float Tick = 0.0f;
};

struct AnimationQuaternionKey
{
	XMFLOAT4 Value;
	float Tick = 0.0f;
};

struct AnimationChannel
{
	std::string NodeName;

	std::vector<AnimationVectorKey> PositionKeys;
	std::vector<AnimationQuaternionKey> RotationKeys;
	std::vector<AnimationVectorKey> ScalingKeys;
};



class AnimationClip
{
public:
	bool Initialize(const std::string& FilePath);
	float GetDurationTicks() const { return DurationTicks; }
	float GetTicksPerSecond() const { return TicksPerSecond; }
	size_t GetChannelCount() const { return Channels.size(); }
	const AnimationChannel* FindChannel(const std::string& NodeName) const { return &Channels.find(NodeName)->second; }

private:
	std::string Name;
	float DurationTicks = 0.0f;
	float TicksPerSecond = 0.0f;

	std::unordered_map<std::string, AnimationChannel> Channels;
};

