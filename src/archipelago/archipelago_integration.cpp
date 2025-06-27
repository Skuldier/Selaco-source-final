/*
===========================================================================
SELACO ARCHIPELAGO INTEGRATION IMPLEMENTATION - archipelago_integration.cpp
===========================================================================
Implementation of the integration layer between Selaco and Archipelago.
===========================================================================
*/

#include "archipelago_integration.h"
#include <iostream>

// =============================================================================
// STATIC MEMBER DEFINITIONS
// =============================================================================

std::unique_ptr<SelacoArchipelagoClient> SelacoArchipelagoIntegration::archipelago_client_ = nullptr;
bool SelacoArchipelagoIntegration::initialized_ = false;
bool SelacoArchipelagoIntegration::enabled_ = true;
std::string SelacoArchipelagoIntegration::status_message_ = "Not connected";
ArchipelagoState SelacoArchipelagoIntegration::last_state_ = ArchipelagoState::Disconnected;

// =============================================================================
// LIFECYCLE MANAGEMENT
// =============================================================================

bool SelacoArchipelagoIntegration::Initialize()
{
    if (initialized_) {
        return true;
    }
    
    if (!enabled_) {
        status_message_ = "Archipelago disabled";
        return false;
    }
    
    try {
        archipelago_client_ = std::make_unique<SelacoArchipelagoClient>();
        
        // Set up callbacks
        archipelago_client_->SetItemReceivedCallback(OnItemReceived);
        archipelago_client_->SetLocationCheckedCallback(OnLocationCheckConfirmed);
        archipelago_client_->SetStateChangedCallback(OnStateChanged);
        archipelago_client_->SetPrintCallback(OnPrintMessage);
        
        if (!archipelago_client_->Initialize()) {
            archipelago_client_ = nullptr;
            status_message_ = "Failed to initialize Archipelago";
            return false;
        }
        
        initialized_ = true;
        status_message_ = "Archipelago initialized";
        
        std::cout << "[SELACO] Archipelago integration initialized successfully" << std::endl;
        return true;
    }
    catch (const std::exception& e) {
        status_message_ = "Initialization error: " + std::string(e.what());
        std::cerr << "[SELACO] Archipelago initialization failed: " << e.what() << std::endl;
        return false;
    }
}

void SelacoArchipelagoIntegration::Shutdown()
{
    if (!initialized_) return;
    
    if (archipelago_client_) {
        archipelago_client_->Shutdown();
        archipelago_client_ = nullptr;
    }
    
    initialized_ = false;
    status_message_ = "Archipelago shut down";
    
    std::cout << "[SELACO] Archipelago integration shut down" << std::endl;
}

void SelacoArchipelagoIntegration::Update()
{
    if (!initialized_ || !archipelago_client_) return;
    
    // This is called every game tick - must be non-blocking!
    archipelago_client_->Update();
    
    // Update status periodically
    static int update_counter = 0;
    if (++update_counter >= 35) { // Update status once per second (assuming 35 FPS)
        UpdateStatusMessage();
        update_counter = 0;
    }
}

// =============================================================================
// CONNECTION INTERFACE
// =============================================================================

bool SelacoArchipelagoIntegration::Connect(const std::string& host, int port, 
                                        const std::string& slot_name, 
                                        const std::string& password)
{
    if (!initialized_ || !archipelago_client_) {
        status_message_ = "Not initialized";
        return false;
    }
    
    std::cout << "[SELACO] Connecting to Archipelago: " << host << ":" << port 
              << " (slot: " << slot_name << ")" << std::endl;
    
    return archipelago_client_->Connect(host, port, slot_name, password);
}

void SelacoArchipelagoIntegration::Disconnect()
{
    if (archipelago_client_) {
        archipelago_client_->Disconnect();
    }
}

// =============================================================================
// GAME INTERFACE
// =============================================================================

void SelacoArchipelagoIntegration::OnLocationChecked(int64_t location_id)
{
    if (!initialized_ || !archipelago_client_) return;
    
    std::cout << "[SELACO] Location checked: " << location_id << std::endl;
    archipelago_client_->CheckLocation(location_id);
}

std::vector<ArchipelagoItem> SelacoArchipelagoIntegration::GetPendingItems()
{
    if (!initialized_ || !archipelago_client_) {
        return {};
    }
    
    return archipelago_client_->GetReceivedItems();
}

// =============================================================================
// STATUS/DEBUG INTERFACE
// =============================================================================

bool SelacoArchipelagoIntegration::IsConnected()
{
    return initialized_ && archipelago_client_ && archipelago_client_->IsConnected();
}

std::string SelacoArchipelagoIntegration::GetStatusMessage()
{
    return status_message_;
}

ArchipelagoState SelacoArchipelagoIntegration::GetState()
{
    if (!initialized_ || !archipelago_client_) {
        return ArchipelagoState::Disconnected;
    }
    return archipelago_client_->GetState();
}

void SelacoArchipelagoIntegration::SendTestMessage()
{
    // TODO: Implement test message functionality
}

void SelacoArchipelagoIntegration::PrintConnectionInfo()
{
    std::cout << "=== Archipelago Connection Info ===" << std::endl;
    std::cout << "Status: " << GetStatusMessage() << std::endl;
    std::cout << "Connected: " << (IsConnected() ? "Yes" : "No") << std::endl;
    if (archipelago_client_) {
        std::cout << "Player ID: " << archipelago_client_->GetPlayerId() << std::endl;
        std::cout << "Received Items: " << archipelago_client_->GetReceivedItems().size() << std::endl;
        std::cout << "Checked Locations: " << archipelago_client_->GetCheckedLocations().size() << std::endl;
    }
}

// =============================================================================
// CALLBACK IMPLEMENTATIONS
// =============================================================================

void SelacoArchipelagoIntegration::OnItemReceived(const ArchipelagoItem& item)
{
    std::cout << "[SELACO] Received item " << item.item_id 
              << " from player " << item.player_id << std::endl;
    
    // TODO: Integrate with Selaco's item system
    // This is where you would give the player the actual item in-game
}

void SelacoArchipelagoIntegration::OnLocationCheckConfirmed(int64_t location_id)
{
    std::cout << "[SELACO] Location check confirmed: " << location_id << std::endl;
}

void SelacoArchipelagoIntegration::OnStateChanged(ArchipelagoState old_state, ArchipelagoState new_state)
{
    std::cout << "[SELACO] Archipelago state changed: " 
              << static_cast<int>(old_state) << " -> " << static_cast<int>(new_state) << std::endl;
    
    last_state_ = new_state;
    UpdateStatusMessage();
}

void SelacoArchipelagoIntegration::OnPrintMessage(const std::string& text, int priority)
{
    std::cout << "[SELACO] Archipelago message (priority " << priority << "): " << text << std::endl;
    
    // TODO: Display this message in Selaco's chat/message system
}

void SelacoArchipelagoIntegration::UpdateStatusMessage()
{
    if (!initialized_ || !archipelago_client_) {
        status_message_ = "Not initialized";
        return;
    }
    
    ArchipelagoState state = archipelago_client_->GetState();
    
    switch (state) {
        case ArchipelagoState::Disconnected:
            status_message_ = "Disconnected";
            break;
        case ArchipelagoState::Connecting:
            status_message_ = "Connecting...";
            break;
        case ArchipelagoState::WaitingForRoomInfo:
            status_message_ = "Waiting for room info...";
            break;
        case ArchipelagoState::WaitingForDataPackage:
            status_message_ = "Waiting for data package...";
            break;
        case ArchipelagoState::Authenticating:
            status_message_ = "Authenticating...";
            break;
        case ArchipelagoState::Connected:
            status_message_ = "Connected (Player " + std::to_string(archipelago_client_->GetPlayerId()) + ")";
            break;
        case ArchipelagoState::Error:
            status_message_ = "Connection error";
            break;
    }
}

// =============================================================================
// C INTERFACE FOR GAME INTEGRATION
// =============================================================================

extern "C" {

bool SelacoArchipelago_Initialize()
{
    return SelacoArchipelagoIntegration::Initialize();
}

void SelacoArchipelago_Shutdown()
{
    SelacoArchipelagoIntegration::Shutdown();
}

void SelacoArchipelago_Update()
{
    SelacoArchipelagoIntegration::Update();
}

void SelacoArchipelago_OnLocationChecked(int location_id)
{
    SelacoArchipelagoIntegration::OnLocationChecked(static_cast<int64_t>(location_id));
}

// Console command implementations
void CMD_ArchipelagoConnect(const char* host, int port, const char* slot_name, const char* password)
{
    std::string host_str = host ? host : "archipelago.gg";
    std::string slot_str = slot_name ? slot_name : "Player";
    std::string pass_str = password ? password : "";
    
    if (SelacoArchipelagoIntegration::Connect(host_str, port, slot_str, pass_str)) {
        std::cout << "Connection initiated..." << std::endl;
    } else {
        std::cout << "Failed to initiate connection" << std::endl;
    }
}

void CMD_ArchipelagoDisconnect()
{
    SelacoArchipelagoIntegration::Disconnect();
    std::cout << "Disconnected from Archipelago" << std::endl;
}

void CMD_ArchipelagoStatus()
{
    std::cout << "Archipelago Status: " << SelacoArchipelagoIntegration::GetStatusMessage() << std::endl;
    std::cout << "Connected: " << (SelacoArchipelagoIntegration::IsConnected() ? "Yes" : "No") << std::endl;
}

void CMD_ArchipelagoTest()
{
    std::cout << "Testing location check with ID 12345..." << std::endl;
    SelacoArchipelagoIntegration::OnLocationChecked(12345);
}

} // extern "C"