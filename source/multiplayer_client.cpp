#include "multiplayer_client.h"

#include <algorithm>
#include <array>
#include <cerrno>
#include <charconv>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <iomanip>
#include <sstream>

#if defined(_WIN32)
#error "El cliente TCP nativo todavía requiere la implementación WinSock"
#else
#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace m3d {
namespace {

std::vector<std::string> splitTabs(const std::string& line)
{
    std::vector<std::string> fields;
    std::size_t begin = 0;
    while (true) {
        const std::size_t end = line.find('\t', begin);
        fields.push_back(line.substr(begin, end == std::string::npos ? end : end - begin));
        if (end == std::string::npos) break;
        begin = end + 1;
    }
    return fields;
}

std::string percentEncode(const std::string& value)
{
    std::ostringstream output;
    output << std::uppercase << std::hex;
    for (const unsigned char character : value) {
        if ((character >= 'a' && character <= 'z') || (character >= 'A' && character <= 'Z') ||
            (character >= '0' && character <= '9') || character == '-' || character == '_' || character == '.') {
            output << character;
        } else if (character == ' ') {
            output << '+';
        } else {
            output << '%' << std::setw(2) << std::setfill('0') << static_cast<int>(character);
        }
    }
    return output.str();
}

std::string percentDecode(const std::string& value)
{
    std::string output;
    output.reserve(value.size());
    for (std::size_t i = 0; i < value.size(); ++i) {
        if (value[i] == '+') output.push_back(' ');
        else if (value[i] == '%' && i + 2 < value.size()) {
            unsigned int byte = 0;
            std::istringstream input(value.substr(i + 1, 2));
            input >> std::hex >> byte;
            output.push_back(static_cast<char>(byte));
            i += 2;
        } else output.push_back(value[i]);
    }
    return output;
}

template <typename Number>
bool parseNumber(const std::string& value, Number& output)
{
    std::istringstream input(value);
    input >> output;
    return !input.fail();
}

std::string roleSpanish(const std::string& role)
{
    if (role == "admin") return "ADMINISTRADOR";
    if (role == "moderator") return "MODERADOR";
    return "JUGADOR";
}

} // namespace

MultiplayerClient::~MultiplayerClient() { disconnect(); }

void MultiplayerClient::connectTo(const std::string& address, const std::string& playerName,
                                  const std::string& token, int characterIndex, int worldIndex)
{
    disconnect();
    {
        std::scoped_lock lock(mutex_);
        state_ = {};
        state_.status = NetworkStatus::Connecting;
        state_.statusText = "CONECTANDO";
        members_.clear();
        remotes_.clear();
        outgoing_.clear();
    }
    running_ = true;
    worker_ = std::thread(&MultiplayerClient::workerMain, this, address, playerName, token, characterIndex, worldIndex);
}

void MultiplayerClient::disconnect()
{
    running_ = false;
    if (worker_.joinable()) worker_.join();
    setStatus(NetworkStatus::Disconnected, "DESCONECTADO");
}

void MultiplayerClient::queueLine(std::string line)
{
    std::scoped_lock lock(mutex_);
    outgoing_.push_back(std::move(line));
}

void MultiplayerClient::sendChat(const std::string& message)
{
    if (!message.empty()) queueLine("CHAT\t" + percentEncode(message));
}

void MultiplayerClient::sendMove(const Vec3& position, float yaw, float speed, float phase,
                                 float sequence, int characterIndex, int worldIndex)
{
    char buffer[352];
    std::snprintf(buffer, sizeof(buffer), "MOVE\t%.5f\t%.5f\t%.5f\t%.5f\t%.5f\t%.5f\t%.5f\t%d\t%d",
                  position.x, position.y, position.z, yaw, speed, phase, sequence, characterIndex, worldIndex);
    queueLine(buffer);
}

void MultiplayerClient::setStatus(NetworkStatus status, std::string text)
{
    std::scoped_lock lock(mutex_);
    state_.status = status;
    state_.statusText = std::move(text);
}

void MultiplayerClient::workerMain(std::string address, std::string playerName,
                                   std::string token, int characterIndex, int worldIndex)
{
    const std::size_t separator = address.rfind(':');
    const std::string host = separator == std::string::npos ? address : address.substr(0, separator);
    const std::string port = separator == std::string::npos ? "7777" : address.substr(separator + 1);

    addrinfo hints{};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    addrinfo* result = nullptr;
    if (getaddrinfo(host.c_str(), port.c_str(), &hints, &result) != 0) {
        setStatus(NetworkStatus::Error, "NO SE PUDO RESOLVER EL SERVIDOR");
        running_ = false;
        return;
    }

    int socketFd = -1;
    for (addrinfo* entry = result; entry; entry = entry->ai_next) {
        socketFd = ::socket(entry->ai_family, entry->ai_socktype, entry->ai_protocol);
        if (socketFd < 0) continue;
        if (::connect(socketFd, entry->ai_addr, entry->ai_addrlen) == 0) break;
        ::close(socketFd);
        socketFd = -1;
    }
    freeaddrinfo(result);
    if (socketFd < 0) {
        setStatus(NetworkStatus::Error, "SERVIDOR NO DISPONIBLE");
        running_ = false;
        return;
    }

    fcntl(socketFd, F_SETFL, fcntl(socketFd, F_GETFL, 0) | O_NONBLOCK);
    std::string hello = "HELLO\t" + percentEncode(playerName) + "\t" + percentEncode(token) + "\t" + std::to_string(characterIndex) + "\t" + std::to_string(worldIndex) + "\n";
    (void)::send(socketFd, hello.data(), hello.size(), 0);
    setStatus(NetworkStatus::Connecting, "AUTENTICANDO");

    std::string receiveBuffer;
    std::array<char, 4096> chunk{};
    auto lastPing = std::chrono::steady_clock::now();
    while (running_) {
        std::deque<std::string> pending;
        {
            std::scoped_lock lock(mutex_);
            pending.swap(outgoing_);
        }
        for (const auto& line : pending) {
            const std::string framed = line + "\n";
            if (::send(socketFd, framed.data(), framed.size(), 0) < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
                running_ = false;
                break;
            }
        }

        fd_set readSet;
        FD_ZERO(&readSet);
        FD_SET(socketFd, &readSet);
        timeval timeout{0, 50000};
        const int ready = select(socketFd + 1, &readSet, nullptr, nullptr, &timeout);
        if (ready > 0 && FD_ISSET(socketFd, &readSet)) {
            const ssize_t received = recv(socketFd, chunk.data(), chunk.size(), 0);
            if (received <= 0) {
                if (received == 0 || (errno != EAGAIN && errno != EWOULDBLOCK)) running_ = false;
            } else {
                receiveBuffer.append(chunk.data(), static_cast<std::size_t>(received));
                while (true) {
                    const std::size_t newline = receiveBuffer.find('\n');
                    if (newline == std::string::npos) break;
                    std::string line = receiveBuffer.substr(0, newline);
                    receiveBuffer.erase(0, newline + 1);
                    if (!line.empty() && line.back() == '\r') line.pop_back();
                    handleLine(line);
                }
            }
        }

        const auto now = std::chrono::steady_clock::now();
        if (now - lastPing > std::chrono::seconds(5)) {
            queueLine("PING");
            lastPing = now;
        }
    }
    ::close(socketFd);
    bool hadError = false;
    {
        std::scoped_lock lock(mutex_);
        hadError = state_.status == NetworkStatus::Error;
    }
    if (!hadError) setStatus(NetworkStatus::Disconnected, "DESCONECTADO");
}

void MultiplayerClient::handleLine(const std::string& line)
{
    const auto fields = splitTabs(line);
    if (fields.empty()) return;
    std::scoped_lock lock(mutex_);
    if (fields[0] == "WELCOME" && fields.size() >= 3) {
        parseNumber(fields[1], state_.localId);
        state_.localRole = fields[2];
        state_.status = NetworkStatus::Connected;
        state_.statusText = "CONECTADO COMO " + roleSpanish(fields[2]);
    } else if (fields[0] == "JOIN" && fields.size() >= 5) {
        LobbyMember member;
        parseNumber(fields[1], member.id);
        member.name = percentDecode(fields[2]);
        member.role = fields[3];
        parseNumber(fields[4], member.character);
        if (fields.size() >= 6) parseNumber(fields[5], member.world);
        members_[member.id] = member;
        if (member.id != state_.localId) {
            RemotePlayer remote;
            remote.id = member.id;
            remote.name = member.name;
            remote.role = member.role;
            remote.character = member.character;
            remote.world = member.world;
            remotes_[member.id] = remote;
        }
    } else if (fields[0] == "LEAVE" && fields.size() >= 2) {
        std::uint64_t id = 0;
        parseNumber(fields[1], id);
        members_.erase(id);
        remotes_.erase(id);
    } else if (fields[0] == "ROLE" && fields.size() >= 3) {
        std::uint64_t id = 0;
        parseNumber(fields[1], id);
        if (auto member = members_.find(id); member != members_.end()) member->second.role = fields[2];
        if (auto remote = remotes_.find(id); remote != remotes_.end()) remote->second.role = fields[2];
        if (id == state_.localId) state_.localRole = fields[2];
    } else if (fields[0] == "MOVE" && fields.size() >= 10) {
        std::uint64_t id = 0;
        parseNumber(fields[1], id);
        if (id == state_.localId) return;
        auto& remote = remotes_[id];
        remote.id = id;
        if (const auto member = members_.find(id); member != members_.end()) {
            remote.name = member->second.name;
            remote.role = member->second.role;
        }
        parseNumber(fields[2], remote.position.x);
        parseNumber(fields[3], remote.position.y);
        parseNumber(fields[4], remote.position.z);
        parseNumber(fields[5], remote.yaw);
        parseNumber(fields[6], remote.speed);
        parseNumber(fields[7], remote.phase);
        parseNumber(fields[9], remote.character);
        if (fields.size() >= 11) parseNumber(fields[10], remote.world);
    } else if (fields[0] == "CHAT" && fields.size() >= 5) {
        state_.messages.push_back({percentDecode(fields[2]), fields[3], percentDecode(fields[4]), false});
    } else if ((fields[0] == "SYSTEM" || fields[0] == "ERROR") && fields.size() >= 2) {
        state_.messages.push_back({"SISTEMA", "admin", percentDecode(fields[1]), true});
    } else if (fields[0] == "ANNOUNCE" && fields.size() >= 4) {
        state_.messages.push_back({percentDecode(fields[1]), fields[2], percentDecode(fields[3]), true});
    } else if (fields[0] == "KICK" && fields.size() >= 2) {
        state_.messages.push_back({"SISTEMA", "admin", percentDecode(fields[1]), true});
        state_.status = NetworkStatus::Error;
        state_.statusText = "EXPULSADO DEL LOBBY";
        running_ = false;
    }
    while (state_.messages.size() > 64) state_.messages.erase(state_.messages.begin());
}

MultiplayerSnapshot MultiplayerClient::snapshot() const
{
    std::scoped_lock lock(mutex_);
    MultiplayerSnapshot result = state_;
    result.members.clear();
    result.remotePlayers.clear();
    for (const auto& [_, member] : members_) result.members.push_back(member);
    for (const auto& [_, remote] : remotes_) result.remotePlayers.push_back(remote);
    std::sort(result.members.begin(), result.members.end(), [](const auto& a, const auto& b) { return a.name < b.name; });
    return result;
}

} // namespace m3d
