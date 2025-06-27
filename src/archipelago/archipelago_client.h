/*
===========================================================================
ARCHIPELAGO CLIENT FOR SELACO - archipelago_client.h
===========================================================================
High-level Archipelago protocol implementation built on the WebSocket client.
Handles the specific message types and flow required for Archipelago integration.

Based on APDOOM patterns and Archipelago protocol documentation.
===========================================================================
*/

#pragma once

#include "selaco_websocket.h"
#include <unordered_set>
#include <vector>

// =============================================================================
// ARCHIPELAGO PROTOCOL TYPES
// =============================================================================

enum class ArchipelagoState
{
    Disconnected,
    Connecting,
    WaitingForRoomInfo,
    WaitingForDataPackage,
    Authenticating,
    Connected,
    Error
};

struct ArchipelagoItem
{
    int64_t item_id;
    int64_t location_id;
    int player_id;
    std::string item_name;
    std::string player_name;
    bool classified = false;
    
    ArchipelagoItem() : item_id(0), location_id(0), player_id(0) {}
};

struct ArchipelagoRoomInfo
{
    std::string seed_name;
    std::vector<std::string> tags;
    bool password_required = false;
    std::unordered_set<std::string> permissions;
    int hint_cost = 10;
    int location_check_points = 1;
    std::string version = "0.5.0";
    
    ArchipelagoRoomInfo() = default;
};

// =============================================================================
// ARCHIPELAGO CLIENT CLASS
// =============================================================================

class SelacoArchipelagoClient
{
public:
    // Callback types for game integration
    using ItemReceivedCallback = std::function<void(const ArchipelagoItem&)>;
    using LocationCheckedCallback = std::function<void(int64_t location_id)>;
    using StateChangedCallback = std::function<void(ArchipelagoState old_state, ArchipelagoState new_state)>;
    using PrintCallback = std::function<void(const std::string& text, int priority)>;

private:
    // Core WebSocket client
    std::unique_ptr<SelacoWebSocketClient> websocket_client_;
    
    // Archipelago state
    std::atomic<ArchipelagoState> ap_state_;
    ArchipelagoRoomInfo room_info_;
    
    // Connection configuration
    WebSocketConfig ws_config_;
    std::string client_uuid_;
    
    // Game state tracking
    std::unordered_set<int64_t> checked_locations_;
    std::vector<ArchipelagoItem> received_items_;
    int player_id_ = 0;
    
    // Message tracking
    std::atomic<bool> data_package_received_;
    std::atomic<bool> connected_received_;
    
    // Callbacks
    ItemReceivedCallback item_callback_;
    LocationCheckedCallback location_callback_;
    StateChangedCallback state_callback_;
    PrintCallback print_callback_;

public:
    // =============================================================================
    // CONSTRUCTOR / DESTRUCTOR
    // =============================================================================
    
    SelacoArchipelagoClient();
    ~SelacoArchipelagoClient();
    
    // =============================================================================
    // CONNECTION MANAGEMENT
    // =============================================================================
    
    bool Initialize();
    void Shutdown();
    
    bool Connect(const std::string& host, int port, const std::string& slot_name, 
                const std::string& password = "");
    void Disconnect();
    
    // Call from game loop - NON-BLOCKING
    void Update();
    
    // =============================================================================
    // ARCHIPELAGO PROTOCOL METHODS
    // =============================================================================
    
    // Send location check
    void CheckLocation(int64_t location_id);
    void CheckMultipleLocations(const std::vector<int64_t>& location_ids);
    
    // Send status update
    void UpdateStatus(int status); // 0=playing, 1=completed, 2=goal
    
    // Send say message (chat)
    void Say(const std::string& message);
    
    // Request hints
    void RequestHint(int64_t location_id);
    
    // =============================================================================
    // STATE QUERIES
    // =============================================================================
    
    ArchipelagoState GetState() const { return ap_state_.load(); }
    bool IsConnected() const { return GetState() == ArchipelagoState::Connected; }
    
    int GetPlayerId() const { return player_id_; }
    const ArchipelagoRoomInfo& GetRoomInfo() const { return room_info_; }
    
    const std::vector<ArchipelagoItem>& GetReceivedItems() const { return received_items_; }
    const std::unordered_set<int64_t>& GetCheckedLocations() const { return checked_locations_; }
    
    // =============================================================================
    // CALLBACK REGISTRATION
    // =============================================================================
    
    void SetItemReceivedCallback(ItemReceivedCallback callback) { item_callback_ = callback; }
    void SetLocationCheckedCallback(LocationCheckedCallback callback) { location_callback_ = callback; }
    void SetStateChangedCallback(StateChangedCallback callback) { state_callback_ = callback; }
    void SetPrintCallback(PrintCallback callback) { print_callback_ = callback; }

private:
    // =============================================================================
    // INTERNAL MESSAGE HANDLERS
    // =============================================================================
    
    void OnWebSocketMessage(const json& message);
    void OnWebSocketStateChanged(WebSocketState old_state, WebSocketState new_state);
    void OnWebSocketError(const std::string& error);
    
    // Archipelago message handlers
    void HandleRoomInfo(const json& packet);
    void HandleDataPackage(const json& packet);
    void HandleConnected(const json& packet);
    void HandleConnectionRefused(const json& packet);
    void HandleReceivedItems(const json& packet);
    void HandleLocationInfo(const json& packet);
    void HandlePrintJSON(const json& packet);
    void HandleRetrieved(const json& packet);
    void HandleSetReply(const json& packet);
    
    // Protocol helpers
    void SendConnect();
    void SendGetDataPackage();
    
    void SetArchipelagoState(ArchipelagoState new_state);
    
    // Utility
    json CreateBasePacket(const std::string& cmd);
    void LogInfo(const std::string& message);
    void LogError(const std::string& message);
};