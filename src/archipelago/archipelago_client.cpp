/*
===========================================================================
ARCHIPELAGO CLIENT IMPLEMENTATION - archipelago_client.cpp
===========================================================================
Implementation of the Archipelago protocol client for Selaco.
===========================================================================
*/

#include "archipelago_client.h"

// =============================================================================
// ARCHIPELAGO CLIENT IMPLEMENTATION
// =============================================================================

SelacoArchipelagoClient::SelacoArchipelagoClient()
    : websocket_client_(std::make_unique<SelacoWebSocketClient>())
    , ap_state_(ArchipelagoState::Disconnected)
    , player_id_(0)
    , data_package_received_(false)
    , connected_received_(false)
{
    client_uuid_ = GenerateClientUUID();
    
    // Set up WebSocket callbacks
    websocket_client_->SetMessageCallback(
        [this](const json& msg) { OnWebSocketMessage(msg); });
    websocket_client_->SetStateCallback(
        [this](WebSocketState old_s, WebSocketState new_s) { OnWebSocketStateChanged(old_s, new_s); });
    websocket_client_->SetErrorCallback(
        [this](const std::string& err) { OnWebSocketError(err); });
        
    LogInfo("SelacoArchipelagoClient created with UUID: " + client_uuid_);
}

SelacoArchipelagoClient::~SelacoArchipelagoClient()
{
    Shutdown();
}

bool SelacoArchipelagoClient::Initialize()
{
    return websocket_client_->Initialize();
}

void SelacoArchipelagoClient::Shutdown()
{
    if (websocket_client_) {
        websocket_client_->Shutdown();
    }
    SetArchipelagoState(ArchipelagoState::Disconnected);
}

bool SelacoArchipelagoClient::Connect(const std::string& host, int port, 
                                     const std::string& slot_name, const std::string& password)
{
    if (!websocket_client_) {
        LogError("WebSocket client not initialized");
        return false;
    }
    
    // Configure connection
    ws_config_.host = host;
    ws_config_.port = port;
    ws_config_.slot_name = slot_name;
    ws_config_.password = password;
    ws_config_.use_ssl = true; // Always use SSL for Archipelago
    
    LogInfo("Connecting to Archipelago server: " + host + ":" + std::to_string(port));
    LogInfo("Slot name: " + slot_name);
    
    SetArchipelagoState(ArchipelagoState::Connecting);
    return websocket_client_->Connect(ws_config_);
}

void SelacoArchipelagoClient::Disconnect()
{
    if (websocket_client_) {
        websocket_client_->Disconnect();
    }
    SetArchipelagoState(ArchipelagoState::Disconnected);
}

void SelacoArchipelagoClient::Update()
{
    if (websocket_client_) {
        websocket_client_->Service();
    }
}

// =============================================================================
// ARCHIPELAGO PROTOCOL IMPLEMENTATION
// =============================================================================

void SelacoArchipelagoClient::OnWebSocketMessage(const json& message)
{
    try {
        // Archipelago messages are arrays of packets
        if (!message.is_array()) {
            LogError("Received non-array message from Archipelago server");
            return;
        }
        
        for (const auto& packet : message) {
            if (!packet.contains("cmd")) {
                LogError("Packet missing 'cmd' field");
                continue;
            }
            
            std::string cmd = packet["cmd"];
            
            if (cmd == "RoomInfo") {
                HandleRoomInfo(packet);
            } else if (cmd == "DataPackage") {
                HandleDataPackage(packet);
            } else if (cmd == "Connected") {
                HandleConnected(packet);
            } else if (cmd == "ConnectionRefused") {
                HandleConnectionRefused(packet);
            } else if (cmd == "ReceivedItems") {
                HandleReceivedItems(packet);
            } else if (cmd == "LocationInfo") {
                HandleLocationInfo(packet);
            } else if (cmd == "PrintJSON") {
                HandlePrintJSON(packet);
            } else if (cmd == "Retrieved") {
                HandleRetrieved(packet);
            } else if (cmd == "SetReply") {
                HandleSetReply(packet);
            } else {
                LogInfo("Unknown command: " + cmd);
            }
        }
    }
    catch (const std::exception& e) {
        LogError("Error processing Archipelago message: " + std::string(e.what()));
    }
}

void SelacoArchipelagoClient::HandleRoomInfo(const json& packet)
{
    LogInfo("Received RoomInfo");
    
    // Parse room information
    if (packet.contains("seed_name")) {
        room_info_.seed_name = packet["seed_name"];
    }
    if (packet.contains("password")) {
        room_info_.password_required = packet["password"];
    }
    
    SetArchipelagoState(ArchipelagoState::WaitingForDataPackage);
    
    // Request data package
    SendGetDataPackage();
}

void SelacoArchipelagoClient::HandleDataPackage(const json& packet)
{
    LogInfo("Received DataPackage");
    data_package_received_ = true;
    
    // TODO: Parse game data from packet if needed
    
    SetArchipelagoState(ArchipelagoState::Authenticating);
    
    // Send connection request
    SendConnect();
}

void SelacoArchipelagoClient::HandleConnected(const json& packet)
{
    LogInfo("Successfully connected to Archipelago!");
    
    // Parse player information
    if (packet.contains("slot")) {
        player_id_ = packet["slot"];
    }
    
    connected_received_ = true;
    SetArchipelagoState(ArchipelagoState::Connected);
}

void SelacoArchipelagoClient::HandleConnectionRefused(const json& packet)
{
    std::string reason = "Unknown reason";
    if (packet.contains("errors")) {
        auto errors = packet["errors"];
        if (errors.is_array() && !errors.empty()) {
            reason = errors[0];
        }
    }
    
    LogError("Connection refused: " + reason);
    SetArchipelagoState(ArchipelagoState::Error);
}

void SelacoArchipelagoClient::HandleReceivedItems(const json& packet)
{
    if (!packet.contains("items")) return;
    
    for (const auto& item_json : packet["items"]) {
        ArchipelagoItem item;
        item.item_id = item_json.value("item", 0);
        item.location_id = item_json.value("location", 0);
        item.player_id = item_json.value("player", 0);
        
        received_items_.push_back(item);
        
        if (item_callback_) {
            item_callback_(item);
        }
        
        LogInfo("Received item: " + std::to_string(item.item_id));
    }
}

void SelacoArchipelagoClient::HandlePrintJSON(const json& packet)
{
    if (packet.contains("text")) {
        std::string text = packet["text"];
        int priority = packet.value("priority", 0);
        
        if (print_callback_) {
            print_callback_(text, priority);
        }
        
        LogInfo("Print: " + text);
    }
}

void SelacoArchipelagoClient::SendConnect()
{
    json connect_packet = CreateBasePacket("Connect");
    connect_packet["password"] = ws_config_.password;
    connect_packet["game"] = ws_config_.game_name;
    connect_packet["name"] = ws_config_.slot_name;
    connect_packet["uuid"] = client_uuid_;
    connect_packet["version"] = json{{"major", 0}, {"minor", 5}, {"build", 0}};
    connect_packet["items_handling"] = 0b111; // All items handling flags
    connect_packet["tags"] = json::array({"CPPClient", "Selaco"});
    
    json message = json::array({connect_packet});
    websocket_client_->SendJSON(message);
    
    LogInfo("Sent Connect packet");
}

void SelacoArchipelagoClient::SendGetDataPackage()
{
    json data_package_packet = CreateBasePacket("GetDataPackage");
    data_package_packet["games"] = json::array({ws_config_.game_name});
    
    json message = json::array({data_package_packet});
    websocket_client_->SendJSON(message);
    
    LogInfo("Sent GetDataPackage packet");
}

void SelacoArchipelagoClient::CheckLocation(int64_t location_id)
{
    if (!IsConnected()) {
        LogError("Cannot check location - not connected");
        return;
    }
    
    checked_locations_.insert(location_id);
    
    json location_check = CreateBasePacket("LocationChecks");
    location_check["locations"] = json::array({location_id});
    
    json message = json::array({location_check});
    websocket_client_->SendJSON(message);
    
    if (location_callback_) {
        location_callback_(location_id);
    }
    
    LogInfo("Checked location: " + std::to_string(location_id));
}

json SelacoArchipelagoClient::CreateBasePacket(const std::string& cmd)
{
    json packet;
    packet["cmd"] = cmd;
    return packet;
}

void SelacoArchipelagoClient::SetArchipelagoState(ArchipelagoState new_state)
{
    ArchipelagoState old_state = ap_state_.exchange(new_state);
    
    if (old_state != new_state && state_callback_) {
        state_callback_(old_state, new_state);
    }
}

void SelacoArchipelagoClient::LogInfo(const std::string& message)
{
    std::cout << "[SELACO-AP] INFO: " << message << std::endl;
}

void SelacoArchipelagoClient::LogError(const std::string& message)
{
    std::cerr << "[SELACO-AP] ERROR: " << message << std::endl;
}

// =============================================================================
// STUB IMPLEMENTATIONS FOR UNUSED HANDLERS
// =============================================================================

void SelacoArchipelagoClient::HandleLocationInfo(const json&) {}
void SelacoArchipelagoClient::HandleRetrieved(const json&) {}
void SelacoArchipelagoClient::HandleSetReply(const json&) {}
void SelacoArchipelagoClient::OnWebSocketStateChanged(WebSocketState, WebSocketState) {}
void SelacoArchipelagoClient::OnWebSocketError(const std::string& error) 
{
    LogError("WebSocket error: " + error);
    SetArchipelagoState(ArchipelagoState::Error);
}

// Additional methods stubs for future implementation
void SelacoArchipelagoClient::CheckMultipleLocations(const std::vector<int64_t>&) {}
void SelacoArchipelagoClient::UpdateStatus(int) {}
void SelacoArchipelagoClient::Say(const std::string&) {}
void SelacoArchipelagoClient::RequestHint(int64_t) {}