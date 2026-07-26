#pragma once

#include "math_types.h"

#include <atomic>
#include <cstdint>
#include <deque>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

namespace m3d {

enum class NetworkStatus
{
    Disconnected,
    Connecting,
    Connected,
    Error,
};

struct LobbyMember
{
    std::uint64_t id = 0;
    std::string name;
    std::string role = "gamer";
    int character = 0;
    int world = 0;
};

struct ChatMessage
{
    std::string sender;
    std::string role;
    std::string text;
    bool system = false;
};

struct RemotePlayer
{
    std::uint64_t id = 0;
    std::string name;
    std::string role = "gamer";
    int character = 0;
    Vec3 position{0.0f, 0.13f, 0.0f};
    float yaw = 0.0f;
    float speed = 0.0f;
    float phase = 0.0f;
    int world = 0;
};

struct MultiplayerSnapshot
{
    NetworkStatus status = NetworkStatus::Disconnected;
    std::string statusText = "DESCONECTADO";
    std::string localRole = "gamer";
    std::uint64_t localId = 0;
    std::vector<LobbyMember> members;
    std::vector<ChatMessage> messages;
    std::vector<RemotePlayer> remotePlayers;
};

class MultiplayerClient
{
public:
    MultiplayerClient() = default;
    ~MultiplayerClient();
    MultiplayerClient(const MultiplayerClient&) = delete;
    MultiplayerClient& operator=(const MultiplayerClient&) = delete;

    void connectTo(const std::string& address, const std::string& playerName,
                   const std::string& token, int characterIndex, int worldIndex);
    void disconnect();
    void sendChat(const std::string& message);
    void sendMove(const Vec3& position, float yaw, float speed, float phase,
                  float sequence, int characterIndex, int worldIndex);
    MultiplayerSnapshot snapshot() const;

private:
    void workerMain(std::string address, std::string playerName,
                    std::string token, int characterIndex, int worldIndex);
    void queueLine(std::string line);
    void handleLine(const std::string& line);
    void setStatus(NetworkStatus status, std::string text);

    std::atomic<bool> running_{false};
    std::thread worker_;
    mutable std::mutex mutex_;
    MultiplayerSnapshot state_;
    std::unordered_map<std::uint64_t, LobbyMember> members_;
    std::unordered_map<std::uint64_t, RemotePlayer> remotes_;
    std::deque<std::string> outgoing_;
};

} // namespace m3d
