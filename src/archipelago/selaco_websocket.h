/*
===========================================================================
SELACO WEBSOCKET CLIENT - selaco_websocket.h
===========================================================================
WebSocket client implementation for Archipelago integration in Selaco.
Based on proven patterns from APDOOM with adaptations for Selaco's architecture.

Key Features:
- Single-threaded, non-blocking operation
- Comprehensive error handling and logging  
- Proper SSL/TLS support
- Memory-safe resource management
- Integration with Selaco's game loop

===========================================================================
*/

#pragma once

#include <libwebsockets.h>
#include <nlohmann/json.hpp>
#include <string>
#include <queue>
#include <mutex>
#include <atomic>
#include <functional>
#include <memory>

using json = nlohmann::json;

// Forward declarations
class SelacoWebSocketClient;

// =============================================================================
// WEBSOCKET CONNECTION STATES
// =============================================================================

enum class WebSocketState
{
    Disconnected,
    Connecting,
    Connected,
    Authenticating,
    Ready,
    Error,
    Disconnecting
};

// =============================================================================
// CONNECTION CONFIGURATION
// =============================================================================

struct WebSocketConfig
{
    std::string host = "archipelago.gg";
    int port = 38281;
    std::string path = "/";
    bool use_ssl = true;
    int timeout_ms = 10000;
    bool validate_certificates = true;
    
    // Archipelago-specific settings
    std::string slot_name;
    std::string game_name = "Selaco";
    std::string password;
    
    WebSocketConfig() = default;
};

// =============================================================================
// SESSION DATA FOR LIBWEBSOCKETS
// =============================================================================

struct SelacoPeerData
{
    SelacoWebSocketClient* client;
    char rx_buffer[8192];
    size_t rx_len;
    bool established;
    
    SelacoPeerData() : client(nullptr), rx_len(0), established(false) 
    {
        memset(rx_buffer, 0, sizeof(rx_buffer));
    }
};

// =============================================================================
// MAIN WEBSOCKET CLIENT CLASS
// =============================================================================

class SelacoWebSocketClient 
{
public:
    // Callback function types
    using MessageCallback = std::function<void(const json&)>;
    using StateCallback = std::function<void(WebSocketState, WebSocketState)>;
    using ErrorCallback = std::function<void(const std::string&)>;

private:
    // LibWebSocket components
    struct lws_context* context_;
    struct lws* websocket_;
    struct lws_context_creation_info context_info_;
    struct lws_client_connect_info connect_info_;
    
    // Connection state
    std::atomic<WebSocketState> state_;
    WebSocketConfig config_;
    
    // Message queuing (thread-safe)
    std::mutex send_queue_mutex_;
    std::queue<std::string> send_queue_;
    std::atomic<bool> service_running_;
    
    // Callbacks
    MessageCallback message_callback_;
    StateCallback state_callback_;
    ErrorCallback error_callback_;
    
    // Statistics
    uint32_t messages_sent_;
    uint32_t messages_received_;
    uint32_t connection_attempts_;
    
    // SSL protocols for libwebsockets
    static const struct lws_protocols protocols_[];

public:
    // =============================================================================
    // CONSTRUCTOR / DESTRUCTOR
    // =============================================================================
    
    SelacoWebSocketClient();
    ~SelacoWebSocketClient();
    
    // Disable copy/move to prevent issues with libwebsockets context
    SelacoWebSocketClient(const SelacoWebSocketClient&) = delete;
    SelacoWebSocketClient& operator=(const SelacoWebSocketClient&) = delete;
    SelacoWebSocketClient(SelacoWebSocketClient&&) = delete;
    SelacoWebSocketClient& operator=(SelacoWebSocketClient&&) = delete;
    
    // =============================================================================
    // CONNECTION MANAGEMENT
    // =============================================================================
    
    bool Initialize();
    void Shutdown();
    
    bool Connect(const WebSocketConfig& config);
    void Disconnect();
    
    // Call this regularly from game loop - NON-BLOCKING
    void Service();
    
    // =============================================================================
    // MESSAGE HANDLING
    // =============================================================================
    
    bool SendJSON(const json& message);
    bool SendString(const std::string& message);
    
    // =============================================================================
    // CALLBACK REGISTRATION
    // =============================================================================
    
    void SetMessageCallback(MessageCallback callback) { message_callback_ = callback; }
    void SetStateCallback(StateCallback callback) { state_callback_ = callback; }
    void SetErrorCallback(ErrorCallback callback) { error_callback_ = callback; }
    
    // =============================================================================
    // STATE QUERIES
    // =============================================================================
    
    WebSocketState GetState() const { return state_.load(); }
    bool IsConnected() const { return GetState() >= WebSocketState::Connected; }
    bool IsReady() const { return GetState() == WebSocketState::Ready; }
    
    const WebSocketConfig& GetConfig() const { return config_; }
    
    // Statistics
    uint32_t GetMessagesSent() const { return messages_sent_; }
    uint32_t GetMessagesReceived() const { return messages_received_; }
    uint32_t GetConnectionAttempts() const { return connection_attempts_; }
    
    // =============================================================================
    // LIBWEBSOCKETS CALLBACK (STATIC)
    // =============================================================================
    
    static int WebSocketCallback(struct lws* wsi, 
                                enum lws_callback_reasons reason,
                                void* user, void* in, size_t len);

private:
    // =============================================================================
    // INTERNAL METHODS
    // =============================================================================
    
    void SetState(WebSocketState new_state);
    void HandleError(const std::string& error_message);
    void ProcessIncomingMessage(const char* data, size_t length);
    void FlushSendQueue();
    
    bool InitializeSSL();
    void CleanupSSL();
    
    // Logging helpers
    void LogInfo(const std::string& message);
    void LogWarning(const std::string& message);
    void LogError(const std::string& message);
};

// =============================================================================
// UTILITY FUNCTIONS
// =============================================================================

// Convert WebSocketState to string for debugging
const char* WebSocketStateToString(WebSocketState state);

// Validate JSON message structure
bool ValidateArchipelagoMessage(const json& message);

// Generate UUID for client identification
std::string GenerateClientUUID();