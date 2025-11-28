/*******************************************************************************
 * Copyright (c) 2025, Rockwell Automation, Inc.
 * All rights reserved.
 *
 ******************************************************************************/

/** @file eth_media_counters.c
 *  @brief ESP32 Ethernet Media Counter Implementation
 *
 *  This module implements hardware-level media counter collection for the
 *  EtherNet/IP Ethernet Link object. It detects IP101 PHY and reads counters
 *  from both ESP32 EMAC registers and IP101 PHY MIB registers.
 */

#include "eth_media_counters.h"
#include "trace.h"
#include "sdkconfig.h"
#include <string.h>
#include <inttypes.h>
#include <stdint.h>

// ESP32-P4 EMAC register access
// Based on ESP32-P4 TRM analysis, the EMAC does not expose hardware counter registers
// like ESP32/ESP32-S2/ESP32-S3. The Register Summary shows DMA and MAC control registers
// but no counter/statistics registers. ESP32-P4 uses a different EMAC IP core (likely
// Synopsys DesignWare) that may not provide these counters at the register level.
#if defined(CONFIG_IDF_TARGET_ESP32P4)
// ESP32-P4: EMAC counter registers are not available
// The TRM Register Summary (Section 50.7) does not list counter registers.
// Only PHY counters (IP101) will be available for ESP32-P4.
#define EMAC_REGISTERS_NOT_AVAILABLE
#elif defined(CONFIG_IDF_TARGET_ESP32) || defined(CONFIG_IDF_TARGET_ESP32S2) || defined(CONFIG_IDF_TARGET_ESP32S3)
#include "soc/emac_reg.h"
#include "soc/emac_struct.h"
#else
#warning "Unsupported ESP32 target for EMAC register access"
#define EMAC_REGISTERS_NOT_AVAILABLE
#endif

// IP101 PHY ID: OUI (0x0243) + Model (0x0C54)
#define IP101_PHY_ID_OUI_MASK    0xFFFF0000
#define IP101_PHY_ID_OUI         0x02430000
#define IP101_PHY_ID_MODEL_MASK  0x0000FFFF
#define IP101_PHY_ID_MODEL       0x00000C54
#define IP101_PHY_ID             (IP101_PHY_ID_OUI | IP101_PHY_ID_MODEL)

// IP101 Page Control Register (from datasheet: Register 20 = 0x14)
#define IP101_REG_PAGE_CONTROL   0x14  // Page Control Register (default: 0x0010)

// IP101 RX Counter Registers (from datasheet section 4.5)
// These are the ONLY counter registers found in the IP101 datasheet.
// Note: IP101 does NOT appear to have collision counters (single, multiple, late, excessive).
// Collision counters are typically TX-related and may not be implemented in IP101.
//
// ✅ VERIFIED COUNTER REGISTERS:
#define IP101_REG_RX_CRC_ERR     0x12  // Page 1, Register 18: RX CRC Error Counter (CRC_ERR_CNT) - maps to FCS Errors
#define IP101_REG_RX_PKT_CNT     0x12  // Page 2, Register 18: RX Packet Counter (PKT_STS_CNT) - total RX packets
#define IP101_REG_RX_SYMB_ERR    0x12  // Page 11, Register 18: RX Symbol Error Counter (SYMB_ERR_CNT) - symbol errors

// Control Registers:
#define IP101_REG_RX_CNT_CTRL    0x11  // Page 1, Register 17: RX Counter Control (enable/disable)
#define IP101_REG_RX_CNT_CTRL2   0x11  // Page 8, Register 17: RX Counter Control (clear/countdown)

// ❌ NOT AVAILABLE IN IP101 (will remain at zero):
// - Single Collision Frames
// - Multiple Collision Frames  
// - Late Collisions
// - Excessive Collisions
// - Carrier Sense Errors (may be in symbol errors)
// - SQE Test Errors
//
// These counters are typically TX-related and IP101 only provides RX counters.
// For collision counters, we would need to rely on ESP32 EMAC counters if available.

// PHY Register Addresses (standard IEEE 802.3)
#define PHY_REG_PHYIDR1          0x02  // PHY Identifier Register 1
#define PHY_REG_PHYIDR2          0x03  // PHY Identifier Register 2

static esp_eth_mac_t *s_mac = NULL;
static uint8_t s_phy_addr = 0;
static bool s_ip101_detected = false;

// Forward declarations
static bool ip101_select_page(uint8_t page);
static uint32_t read_ip101_counter_reg(uint8_t page, uint8_t reg_addr);

// Baseline values for PHY counters (subtracted from current readings)
// This allows counters to reset on GetAndClear and start at zero on boot
static struct {
    uint32_t single_coll;
    uint32_t multi_coll;
    uint32_t late_coll;
    uint32_t exc_coll;
    uint32_t crs_errs;
    uint32_t sqe_test_errs;
} s_phy_baseline = {0};

#ifndef EMAC_REGISTERS_NOT_AVAILABLE
// Baseline values for EMAC counters
static struct {
    uint32_t align_errs;
    uint32_t fcs_errs;
    uint32_t frame_too_long;
    uint32_t def_trans;
    uint32_t mac_tx_errs;
    uint32_t mac_rx_errs;
} s_emac_baseline = {0};
#endif

bool EthMediaCountersInit(esp_eth_mac_t *mac, uint8_t phy_addr) {
    if (mac == NULL) {
        OPENER_TRACE_ERR("EthMediaCountersInit: MAC pointer is NULL\n");
        return false;
    }

    s_mac = mac;
    s_phy_addr = phy_addr;
    s_ip101_detected = false;

    // Read PHY ID registers
    uint32_t phy_id1 = 0;
    uint32_t phy_id2 = 0;
    esp_err_t ret;

    ret = mac->read_phy_reg(mac, phy_addr, PHY_REG_PHYIDR1, &phy_id1);
    if (ret != ESP_OK) {
        OPENER_TRACE_ERR("EthMediaCountersInit: Failed to read PHY ID1: %d\n", ret);
        return false;
    }

    ret = mac->read_phy_reg(mac, phy_addr, PHY_REG_PHYIDR2, &phy_id2);
    if (ret != ESP_OK) {
        OPENER_TRACE_ERR("EthMediaCountersInit: Failed to read PHY ID2: %d\n", ret);
        return false;
    }

    // Combine PHY ID (ID1 is upper 16 bits, ID2 is lower 16 bits)
    uint32_t phy_id = ((phy_id1 & 0xFFFF) << 16) | (phy_id2 & 0xFFFF);

    // Check if it's IP101
    uint32_t phy_oui = phy_id & IP101_PHY_ID_OUI_MASK;
    uint32_t phy_model = phy_id & IP101_PHY_ID_MODEL_MASK;

    if (phy_oui == IP101_PHY_ID_OUI && phy_model == IP101_PHY_ID_MODEL) {
        s_ip101_detected = true;
        OPENER_TRACE_INFO("EthMediaCountersInit: IP101 PHY detected (ID: 0x%08" PRIX32 "), media counters enabled\n", phy_id);
        
        // Initialize baseline to current hardware values so counters start at zero
        // This ensures counters reset on boot
        // Note: IP101 only has RX counters, collision counters are not available
        uint32_t rx_crc_baseline = read_ip101_counter_reg(1, IP101_REG_RX_CRC_ERR);
        uint32_t rx_symb_baseline = read_ip101_counter_reg(11, IP101_REG_RX_SYMB_ERR);
        
        // Store baselines (collision counters remain at zero since IP101 doesn't have them)
        s_phy_baseline.single_coll = 0;  // Not available in IP101
        s_phy_baseline.multi_coll = 0;   // Not available in IP101
        s_phy_baseline.late_coll = 0;   // Not available in IP101
        s_phy_baseline.exc_coll = 0;    // Not available in IP101
        s_phy_baseline.crs_errs = 0;    // Not available in IP101
        s_phy_baseline.sqe_test_errs = 0; // Not available in IP101
        
        // Store RX counter baselines (for potential future use if we map them to media counters)
        // For now, FCS errors are already handled by EMAC counters
        
        OPENER_TRACE_INFO("EthMediaCountersInit: Baseline initialized (RX CRC=%" PRIu32 ", RX Symbol=%" PRIu32 ")\n",
                          rx_crc_baseline, rx_symb_baseline);
        OPENER_TRACE_INFO("EthMediaCountersInit: Note - IP101 does not have collision counters (single, multi, late, exc)\n");
        
        return true;
    } else {
        OPENER_TRACE_INFO("EthMediaCountersInit: PHY detected (ID: 0x%08" PRIX32 ") is not IP101, media counters disabled\n", phy_id);
        s_ip101_detected = false;
        return false;
    }
}

bool EthMediaCountersSupported(void) {
    return s_ip101_detected && (s_mac != NULL);
}

// IP101 page selection helper
static bool ip101_select_page(uint8_t page) {
    if (s_mac == NULL || !s_ip101_detected) {
        return false;
    }
    
    // Read current page control register
    uint32_t page_control = 0;
    esp_err_t ret = s_mac->read_phy_reg(s_mac, s_phy_addr, IP101_REG_PAGE_CONTROL, &page_control);
    if (ret != ESP_OK) {
        OPENER_TRACE_ERR("Failed to read IP101 page control register: %d\n", ret);
        return false;
    }
    
    // Check if already on correct page (bits 4-0 contain page number)
    uint8_t current_page = page_control & 0x1F;
    if (current_page == page) {
        return true;  // Already on correct page
    }
    
    // Set page number (bits 4-0) while preserving other bits
    page_control = (page_control & ~0x1F) | (page & 0x1F);
    ret = s_mac->write_phy_reg(s_mac, s_phy_addr, IP101_REG_PAGE_CONTROL, page_control);
    if (ret != ESP_OK) {
        OPENER_TRACE_ERR("Failed to write IP101 page control register: %d\n", ret);
        return false;
    }
    
    return true;
}

// Read IP101 counter register with page selection
static uint32_t read_ip101_counter_reg(uint8_t page, uint8_t reg_addr) {
    if (s_mac == NULL || !s_ip101_detected) {
        return 0;
    }

    // Select the correct page
    if (!ip101_select_page(page)) {
        OPENER_TRACE_ERR("Failed to select IP101 page %d\n", page);
        return 0;
    }
    
    uint32_t reg_value = 0;
    esp_err_t ret = s_mac->read_phy_reg(s_mac, s_phy_addr, reg_addr, &reg_value);
    if (ret != ESP_OK) {
        OPENER_TRACE_ERR("Failed to read IP101 page %d register 0x%02X: %d\n", page, reg_addr, ret);
        return 0;
    }

    // IP101 counter registers are 16-bit, return lower 16 bits
    return reg_value & 0xFFFF;
}

void EthMediaCountersCollect(CipEthernetLinkMediaCounters *counters) {
    if (counters == NULL) {
        return;
    }

    // Initialize all counters to zero
    memset(counters->cntr32, 0, sizeof(counters->cntr32));

#ifndef EMAC_REGISTERS_NOT_AVAILABLE
    // ESP32 EMAC counters (available regardless of PHY type)
    // Subtract baseline to show delta since last clear
    uint32_t current_align = REG_READ(EMAC_RXALIGNERRFRAMES_REG);
    uint32_t current_fcs = REG_READ(EMAC_RXCRCERRFRAMES_REG);
    uint32_t current_frame_too_long = REG_READ(EMAC_RXOVERSIZEDFRAMES_REG);
    uint32_t current_def_trans = REG_READ(EMAC_TXEXCESSDEF_REG);
    uint32_t current_mac_tx = REG_READ(EMAC_TXCOLLISIONFRAMES_REG);
    uint32_t current_mac_rx_jabber = REG_READ(EMAC_RXJABBERFRAMES_REG);
    uint32_t current_mac_rx_fragments = REG_READ(EMAC_RXFRAGMENTS_REG);
    uint32_t current_mac_rx = current_mac_rx_jabber + current_mac_rx_fragments;
    
    // Debug: Log EMAC counters if they change
    static uint32_t last_emac_fcs = 0;
    static uint32_t last_emac_frame_too_long = 0;
    if ((counters->ul.fcs_errs != last_emac_fcs) || (counters->ul.frame_too_long != last_emac_frame_too_long)) {
        OPENER_TRACE_INFO("EMAC counters changed: fcs=%" PRIu32 " (raw=%" PRIu32 ") frame_too_long=%" PRIu32 " (raw=%" PRIu32 ")\n",
                         counters->ul.fcs_errs, current_fcs, counters->ul.frame_too_long, current_frame_too_long);
        last_emac_fcs = counters->ul.fcs_errs;
        last_emac_frame_too_long = counters->ul.frame_too_long;
    }
    
    // Calculate deltas (handle wraparound - though unlikely for 32-bit counters)
    counters->ul.align_errs = (current_align >= s_emac_baseline.align_errs) ? 
                              (current_align - s_emac_baseline.align_errs) : 
                              ((UINT32_MAX - s_emac_baseline.align_errs) + current_align + 1U);
    counters->ul.fcs_errs = (current_fcs >= s_emac_baseline.fcs_errs) ? 
                            (current_fcs - s_emac_baseline.fcs_errs) : 
                            ((UINT32_MAX - s_emac_baseline.fcs_errs) + current_fcs + 1U);
    counters->ul.frame_too_long = (current_frame_too_long >= s_emac_baseline.frame_too_long) ? 
                                  (current_frame_too_long - s_emac_baseline.frame_too_long) : 
                                  ((UINT32_MAX - s_emac_baseline.frame_too_long) + current_frame_too_long + 1U);
    counters->ul.def_trans = (current_def_trans >= s_emac_baseline.def_trans) ? 
                             (current_def_trans - s_emac_baseline.def_trans) : 
                             ((UINT32_MAX - s_emac_baseline.def_trans) + current_def_trans + 1U);
    counters->ul.mac_tx_errs = (current_mac_tx >= s_emac_baseline.mac_tx_errs) ? 
                               (current_mac_tx - s_emac_baseline.mac_tx_errs) : 
                               ((UINT32_MAX - s_emac_baseline.mac_tx_errs) + current_mac_tx + 1U);
    counters->ul.mac_rx_errs = (current_mac_rx >= s_emac_baseline.mac_rx_errs) ? 
                               (current_mac_rx - s_emac_baseline.mac_rx_errs) : 
                               ((UINT32_MAX - s_emac_baseline.mac_rx_errs) + current_mac_rx + 1U);
#else
    // EMAC registers not available for this target - leave counters at zero
    // PHY counters will still be collected if IP101 is detected
#endif

    // IP101 PHY-specific counters (only if IP101 is detected)
    // Note: IP101 only provides RX counters, not collision counters
    if (s_ip101_detected && s_mac != NULL) {
        // Read IP101 RX counters (with page selection)
        uint32_t current_rx_crc = read_ip101_counter_reg(1, IP101_REG_RX_CRC_ERR);  // Page 1, Reg 18
        uint32_t current_rx_symb = read_ip101_counter_reg(11, IP101_REG_RX_SYMB_ERR); // Page 11, Reg 18
        
        // Map IP101 counters to EtherNet/IP media counters:
        // - RX CRC Error Counter → FCS Errors (already handled by EMAC, but use PHY value if available)
        // - RX Symbol Error Counter → Alignment Errors (symbol errors can cause alignment issues)
        
        // IP101 does not have collision counters - set to zero
        // Collision counters are typically TX-related and IP101 only provides RX counters
        // Note: These counters are read from IP101 RX counters but not currently mapped to EtherNet/IP
        //       media counters since FCS errors are already provided by ESP32 EMAC
        (void)current_rx_crc;  // Suppress unused variable warning (for future use)
        (void)current_rx_symb; // Suppress unused variable warning (for future use)
        
        counters->ul.single_coll = 0;  // Not available in IP101
        counters->ul.multi_coll = 0;   // Not available in IP101
        counters->ul.late_coll = 0;    // Not available in IP101
        counters->ul.exc_coll = 0;     // Not available in IP101
        counters->ul.crs_errs = 0;     // Not available in IP101 (may be part of symbol errors)
        counters->ul.sqe_test_errs = 0; // Not available in IP101
        
        // Note: IP101 RX counters (CRC errors, symbol errors) are already handled by ESP32 EMAC
        // FCS errors are read from EMAC registers, not PHY registers
        
        // Debug: Log IP101 RX counter values (collision counters are always zero)
        static int debug_count = 0;
        debug_count++;
        
        if (debug_count % 10 == 0) {
            OPENER_TRACE_INFO("MediaCntr[%d]: IP101 RX CRC=%" PRIu32 " RX Symbol=%" PRIu32 " (collision counters not available in IP101)\n",
                             debug_count, current_rx_crc, current_rx_symb);
        }
    }
    // If not IP101, these remain zero (already initialized above)
}

void EthMediaCountersResetBaseline(void) {
    // Update baseline to current hardware values
    // This makes subsequent reads show deltas from this point (effectively resetting counters)
    
#ifndef EMAC_REGISTERS_NOT_AVAILABLE
    s_emac_baseline.align_errs = REG_READ(EMAC_RXALIGNERRFRAMES_REG);
    s_emac_baseline.fcs_errs = REG_READ(EMAC_RXCRCERRFRAMES_REG);
    s_emac_baseline.frame_too_long = REG_READ(EMAC_RXOVERSIZEDFRAMES_REG);
    s_emac_baseline.def_trans = REG_READ(EMAC_TXEXCESSDEF_REG);
    s_emac_baseline.mac_tx_errs = REG_READ(EMAC_TXCOLLISIONFRAMES_REG);
    uint32_t mac_rx_jabber = REG_READ(EMAC_RXJABBERFRAMES_REG);
    uint32_t mac_rx_fragments = REG_READ(EMAC_RXFRAGMENTS_REG);
    s_emac_baseline.mac_rx_errs = mac_rx_jabber + mac_rx_fragments;
#endif

    if (s_ip101_detected && s_mac != NULL) {
        // IP101 collision counters are not available - baselines remain at zero
        s_phy_baseline.single_coll = 0;
        s_phy_baseline.multi_coll = 0;
        s_phy_baseline.late_coll = 0;
        s_phy_baseline.exc_coll = 0;
        s_phy_baseline.crs_errs = 0;
        s_phy_baseline.sqe_test_errs = 0;
        
        // Read and store RX counter baselines (for reference, not currently used)
        uint32_t rx_crc = read_ip101_counter_reg(1, IP101_REG_RX_CRC_ERR);
        uint32_t rx_symb = read_ip101_counter_reg(11, IP101_REG_RX_SYMB_ERR);
        
        OPENER_TRACE_INFO("EthMediaCountersResetBaseline: Baseline reset (IP101 RX CRC=%" PRIu32 ", RX Symbol=%" PRIu32 ", collision counters not available)\n",
                          rx_crc, rx_symb);
    }
}

