/*
===========================================================================
SELACO ARCHIPELAGO INTEGRATION - archipelago_integration.h
===========================================================================
Integration layer between Selaco's game systems and the Archipelago client.
This file handles the interface between the game loop and Archipelago.

Key Integration Points:
1. Initialization during game startup
2. Update calls during game loop
3. Console commands for testing
4. Game event hooks

===========================================================================
*/

#pragma once

#include "archipelago_client.h"
#include <memory>

// =============================================================================
// INTEGRATION INTERFACE
// =============================================================================

class SelacoArchipelagoIntegration
{
private:
    static std::unique_ptr<SelacoArchipelagoClient> archipelago_client_;
    static bool initialized_;
    static bool enabled_;
    
    // Connection status for UI display
    static std::string status_message_;
    static ArchipelagoState last_state_;

public:
    // =============================================================================
    // LIFECYCLE MANAGEMENT
    // =============================================================================
    
    // Call during game initialization
    static bool Initialize();
    
    // Call during game shutdown
    static void Shutdown();
    
    // Call every game tick - NON-BLOCKING
    static void Update();
    
    // =============================================================================
    // CONNECTION INTERFACE
    // =============================================================================
    
    // Connect to Archipelago server
    static bool Connect(const std::string& host, int port, 
                       const std::string& slot_name, 
                       const std::string& password = "");
    
    // Disconnect from server
    static void Disconnect();
    
    // =============================================================================
    // GAME INTERFACE
    // =============================================================================
    
    // Called when player picks up an item at a location
    static void OnLocationChecked(int64_t location_id);
    
    // Get items received from other players
    static std::vector<ArchipelagoItem> GetPendingItems();
    
    // =============================================================================
    // STATUS/DEBUG INTERFACE
    // =============================================================================
    
    static bool IsConnected();
    static std::string GetStatusMessage();
    static ArchipelagoState GetState();
    
    // For debugging/testing
    static void SendTestMessage();
    static void PrintConnectionInfo();

private:
    // Callback handlers
    static void OnItemReceived(const ArchipelagoItem& item);
    static void OnLocationCheckConfirmed(int64_t location_id);
    static void OnStateChanged(ArchipelagoState old_state, ArchipelagoState new_state);
    static void OnPrintMessage(const std::string& text, int priority);
    
    static void UpdateStatusMessage();
};

// =============================================================================
// CONSOLE COMMANDS FOR TESTING
// =============================================================================

// Console command handlers - these will be registered with Selaco's console system
void CMD_ArchipelagoConnect(const std::vector<std::string>& args);
void CMD_ArchipelagoDisconnect();
void CMD_ArchipelagoStatus();
void CMD_ArchipelagoTest();

// =============================================================================
// GAME LOOP HOOKS
// =============================================================================

// Hook into D_DoomLoop - call this from the main game loop
extern "C" void SelacoArchipelago_Update();

// Hook into game initialization - call this during startup
extern "C" bool SelacoArchipelago_Initialize();

// Hook into game shutdown - call this during cleanup
extern "C" void SelacoArchipelago_Shutdown();

// Game event hooks
extern "C" void SelacoArchipelago_OnLocationChecked(int location_id);

// C interface console commands
extern "C" void CMD_ArchipelagoConnect(const char* host, int port, const char* slot_name, const char* password);
extern "C" void CMD_ArchipelagoDisconnect();
extern "C" void CMD_ArchipelagoStatus();
extern "C" void CMD_ArchipelagoTest();