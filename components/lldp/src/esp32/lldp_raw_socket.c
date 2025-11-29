/*
 * Copyright (c) 2025, Adam G. Sweeney <agsweeney@gmail.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include "lldp_raw_socket.h"
#include "trace.h"
#include "lwip/netif.h"
#include "lwip/ip4_addr.h"
#include <string.h>
#include <stdbool.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <errno.h>

#include "esp_idf_version.h"

#if ESP_IDF_VERSION_MAJOR >= 5
#include "esp_netif.h"
#include "esp_netif_net_stack.h"
#include "esp_vfs_l2tap.h"
#include "esp_eth.h"
#include "esp_log.h"
#else
#error "LLDP requires ESP-IDF v5.0+ for L2 TAP support. Current version is too old."
#endif

static const char *TAG_RAW = "LLDP_RAW";

// LLDP EtherType: 0x88CC (IEEE 802.1AB)
#define LLDP_ETHERTYPE 0x88CC

static int s_l2tap_fd = -1;
static struct netif *s_netif = NULL;
static bool s_l2tap_registered = false;

/**
 * Initialize raw Ethernet socket for LLDP using ESP-NETIF L2 TAP VFS
 * @param netif Network interface to attach to
 * @return 0 on success, -1 on failure
 */
int lldp_raw_socket_init(struct netif *netif) {
    if (netif == NULL) {
        OPENER_TRACE_ERR("lldp_raw_socket_init: Invalid netif\n");
        return -1;
    }
    
    if (s_l2tap_fd >= 0) {
        ESP_LOGD(TAG_RAW, "Already initialized (fd=%d), skipping", s_l2tap_fd);
        return 0;  // Already initialized
    }
    
    ESP_LOGD(TAG_RAW, "Starting raw socket initialization...");
    s_netif = netif;
    
    // Register L2 TAP VFS driver if not already registered
    // Note: It may have been registered earlier in app_main() before Ethernet starts
    if (!s_l2tap_registered) {
        esp_err_t ret = esp_vfs_l2tap_intf_register(NULL);  // Use default path /dev/net/tap
        if (ret == ESP_ERR_INVALID_STATE) {
            // Already registered (e.g., from app_main) - this is OK
            ESP_LOGD(TAG_RAW, "L2 TAP VFS already registered (expected if registered early)");
            s_l2tap_registered = true;
        } else if (ret != ESP_OK) {
            ESP_LOGE(TAG_RAW, "Failed to register L2 TAP VFS: %s", esp_err_to_name(ret));
            OPENER_TRACE_ERR("lldp_raw_socket_init: Failed to register L2 TAP VFS: %d\n", ret);
            return -1;
        } else {
            s_l2tap_registered = true;
            ESP_LOGD(TAG_RAW, "L2 TAP VFS registered successfully");
        }
    }
    
    // Open L2 TAP device (non-blocking for receive)
    // Note: Following ESP-IDF example pattern - open socket after Ethernet is fully up
    s_l2tap_fd = open("/dev/net/tap", O_RDWR | O_NONBLOCK);
    if (s_l2tap_fd < 0) {
        ESP_LOGE(TAG_RAW, "Unable to open L2 TAP interface: errno %d", errno);
        OPENER_TRACE_ERR("lldp_raw_socket_init: Failed to open /dev/net/tap: %d\n", errno);
        return -1;
    }
    
    // Get ESP-NETIF handle to find the Ethernet driver handle
    esp_netif_t *esp_netif = esp_netif_get_handle_from_netif_impl(netif);
    if (esp_netif == NULL) {
        ESP_LOGE(TAG_RAW, "Failed to get esp_netif handle from netif=%p", netif);
        OPENER_TRACE_ERR("lldp_raw_socket_init: Failed to get esp_netif handle\n");
        close(s_l2tap_fd);
        s_l2tap_fd = -1;
        return -1;
    }
    
    // Verify Ethernet driver is ready - critical for L2 TAP binding
    esp_netif_iodriver_handle driver_handle = esp_netif_get_io_driver(esp_netif);
    if (driver_handle == NULL) {
        ESP_LOGE(TAG_RAW, "Ethernet driver handle is NULL - driver not ready yet!");
        OPENER_TRACE_ERR("lldp_raw_socket_init: Ethernet driver handle is NULL\n");
        close(s_l2tap_fd);
        s_l2tap_fd = -1;
        return -1;
    }
    
    // Bind to the interface - try "ETH_DEF" first (as per ESP-IDF example)
    const char *if_key = "ETH_DEF";
    int ret;
    if ((ret = ioctl(s_l2tap_fd, L2TAP_S_INTF_DEVICE, if_key)) == -1) {
        ESP_LOGE(TAG_RAW, "Unable to bind L2 TAP fd %d with Ethernet device: errno %d", s_l2tap_fd, errno);
        OPENER_TRACE_ERR("lldp_raw_socket_init: Failed to bind to interface\n");
        close(s_l2tap_fd);
        s_l2tap_fd = -1;
        return -1;
    }
    
    // Verify driver handle binding (as per ESP-IDF example)
    esp_netif_iodriver_handle bound_driver_handle = NULL;
    if (ioctl(s_l2tap_fd, L2TAP_G_DEVICE_DRV_HNDL, &bound_driver_handle) < 0) {
        ESP_LOGE(TAG_RAW, "Failed to get socket eth_handle: errno %d", errno);
        OPENER_TRACE_ERR("LLDP: Failed to get driver handle from socket\n");
        close(s_l2tap_fd);
        s_l2tap_fd = -1;
        return -1;
    }
    
    // Set EtherType filter for reception (0x88CC = LLDP)
    // This tells L2 TAP to deliver frames with this EtherType to our socket
    uint16_t lldp_ethertype = 0x88CC;
    if ((ret = ioctl(s_l2tap_fd, L2TAP_S_RCV_FILTER, &lldp_ethertype)) != 0) {
        ESP_LOGE(TAG_RAW, "Unable to configure fd %d Ethernet type receive filter: errno %d", s_l2tap_fd, errno);
        OPENER_TRACE_ERR("LLDP: CRITICAL - Failed to set EtherType filter! errno=%d (%s)\n", 
                         errno, strerror(errno));
        close(s_l2tap_fd);
        s_l2tap_fd = -1;
        return -1;  // Fail initialization if filter can't be set
    }
    
    // Verify filter was set correctly (only log errors)
    uint16_t read_back_filter = 0;
    if (ioctl(s_l2tap_fd, L2TAP_G_RCV_FILTER, &read_back_filter) == 0) {
        if (read_back_filter != lldp_ethertype) {
            ESP_LOGE(TAG_RAW, "Filter mismatch! Set=0x%04x, Read=0x%04x", 
                     lldp_ethertype, read_back_filter);
        }
    }
    
    // Final verification: Read back socket configuration to confirm everything is set correctly
    esp_netif_iodriver_handle final_driver_handle = NULL;
    uint16_t final_filter = 0;
    
    if (ioctl(s_l2tap_fd, L2TAP_G_DEVICE_DRV_HNDL, &final_driver_handle) == 0) {
        if (final_driver_handle != driver_handle) {
            ESP_LOGE(TAG_RAW, "Driver handle mismatch! Expected: %p, Got: %p", 
                     driver_handle, final_driver_handle);
        }
    } else {
        ESP_LOGE(TAG_RAW, "Failed to read back driver handle!");
    }
    
    if (ioctl(s_l2tap_fd, L2TAP_G_RCV_FILTER, &final_filter) == 0) {
        if (final_filter != 0x88CC) {
            ESP_LOGE(TAG_RAW, "Filter mismatch! Expected: 0x88CC, Got: 0x%04x", final_filter);
        }
    } else {
        ESP_LOGE(TAG_RAW, "Failed to read back filter!");
    }
    
    // CRITICAL CHECK: If driver handle is NULL or filter is wrong, reception will NOT work
    if (final_driver_handle == NULL) {
        ESP_LOGE(TAG_RAW, "Driver handle is NULL! Reception will NOT work!");
        close(s_l2tap_fd);
        s_l2tap_fd = -1;
        return -1;
    }
    if (final_filter != 0x88CC) {
        ESP_LOGE(TAG_RAW, "Filter is wrong! Reception will NOT work!");
        close(s_l2tap_fd);
        s_l2tap_fd = -1;
        return -1;
    }
    
    // L2 TAP initialized successfully
    ESP_LOGI(TAG_RAW, "LLDP raw socket initialized successfully (fd=%d)", s_l2tap_fd);
    s_netif = netif;
    return 0;
}

/**
 * Send raw Ethernet frame
 * @param frame Buffer containing complete Ethernet frame (MAC + EtherType + payload)
 * @param len Length of frame (must be >= 14 bytes for MAC header)
 * @return Number of bytes sent on success, -1 on failure
 */
int lldp_raw_socket_send(const uint8_t *frame, size_t len) {
    if (s_l2tap_fd < 0) {
        OPENER_TRACE_ERR("lldp_raw_socket_send: L2 TAP not initialized\n");
        return -1;
    }
    
    if (frame == NULL || len < 14) {  // Minimum Ethernet frame header (14 bytes: 6+6 MAC + 2 EtherType)
        OPENER_TRACE_ERR("lldp_raw_socket_send: Invalid frame or length (len=%zu)\n", len);
        return -1;
    }
    
    // Minimum Ethernet frame size is 60 bytes (excluding FCS which is added by hardware)
    // Pad frame if necessary to meet minimum size requirement
    static uint8_t padded_frame[512] = {0};  // Zero-initialize static buffer
    size_t padded_len = len;
    
    if (len < 60) {
        // Pad frame to minimum Ethernet size (60 bytes)
        if (len > sizeof(padded_frame)) {
            return -1;  // Frame too large
        }
        memcpy(padded_frame, frame, len);
        memset(padded_frame + len, 0, 60 - len);  // Pad with zeros
        padded_len = 60;
        frame = padded_frame;
    }
    
    // Write frame to L2 TAP device
    // NOTE: NO LOGGING in this function - it's called from timer callback
    // Timer callback has very limited stack (2048 bytes), and ESP_LOGI with
    // format strings can easily cause stack overflow
    
    ssize_t written = write(s_l2tap_fd, frame, padded_len);
    
    if (written < 0) {
        // Write failed - check errno
        int write_errno = errno;
        if (write_errno == EBADMSG) {
            // EtherType filter mismatch (shouldn't happen if no filter set)
            return -1;
        } else if (write_errno == EBADF) {
            // Bad file descriptor - socket closed
            return -1;
        } else if (write_errno == ENODEV) {
            // Device not available
            return -1;
        }
        // Unknown error
        return -1;
    }
    
    if ((size_t)written != padded_len) {
        // Partial write - shouldn't happen for Ethernet frames
        return -1;
    }
    
    // Success - write() returned success, but this doesn't guarantee
    // the frame was actually transmitted. The Ethernet driver may
    // queue it or drop it silently. However, since write() succeeds,
    // we assume it was accepted by the driver.
    
    return (int)written;
}

/**
 * Receive raw Ethernet frame (non-blocking)
 * @param frame Buffer to store received frame
 * @param max_len Maximum buffer size
 * @return Number of bytes received, 0 if no frame available, -1 on error
 */
int lldp_raw_socket_recv(uint8_t *frame, size_t max_len) {
    if (s_l2tap_fd < 0) {
        OPENER_TRACE_WARN("LLDP: Raw socket not initialized (fd=%d)\n", s_l2tap_fd);
        return -1;
    }
    
    if (frame == NULL || max_len == 0) {
        OPENER_TRACE_WARN("LLDP: Invalid receive buffer\n");
        return -1;
    }
    
    // Read frame from L2 TAP device (non-blocking)
    ssize_t received = read(s_l2tap_fd, frame, max_len);
    if (received < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return 0;  // No frame available (non-blocking)
        }
        OPENER_TRACE_WARN("LLDP: Read error: errno=%d (%s)\n", errno, strerror(errno));
        return -1;
    }
    
    if (received > 0) {
        // Log first few bytes for debugging
        OPENER_TRACE_WARN("LLDP: Raw socket read %d bytes, MAC: %02x:%02x:%02x:%02x:%02x:%02x -> %02x:%02x:%02x:%02x:%02x:%02x\n",
                          (int)received,
                          frame[0], frame[1], frame[2], frame[3], frame[4], frame[5],
                          frame[6], frame[7], frame[8], frame[9], frame[10], frame[11]);
    }
    
    return (int)received;
}

/**
 * Cleanup raw Ethernet socket
 */
void lldp_raw_socket_deinit(void) {
    if (s_l2tap_fd >= 0) {
        close(s_l2tap_fd);
        s_l2tap_fd = -1;
        // L2 TAP closed (logging removed for production)
    }
    
    // Note: We don't unregister the VFS driver here as it might be used by other components
    // The VFS driver will be cleaned up when the application exits
    
    s_netif = NULL;
}

/**
 * Get MAC address from network interface
 * @param mac Output buffer for MAC address (6 bytes)
 */
void lldp_get_mac_address(uint8_t mac[6]) {
    if (s_netif != NULL) {
        memcpy(mac, s_netif->hwaddr, 6);
    } else {
        // Return zeros if not initialized
        memset(mac, 0, 6);
    }
}

/**
 * Check if raw socket is initialized
 * @return true if initialized, false otherwise
 */
bool lldp_raw_socket_is_initialized(void) {
    return (s_l2tap_fd >= 0);
}

/**
 * Get system hostname from network interface
 * @param hostname Buffer to store hostname (must be at least max_len bytes)
 * @param max_len Maximum length of hostname buffer
 * @return Length of hostname (0 if not available)
 */
size_t lldp_get_hostname(char *hostname, size_t max_len) {
    if (hostname == NULL || max_len == 0) {
        return 0;
    }
    
    if (s_netif == NULL) {
        hostname[0] = '\0';
        return 0;
    }
    
    const char *hostname_ptr = netif_get_hostname(s_netif);
    if (hostname_ptr == NULL || *hostname_ptr == '\0') {
        hostname[0] = '\0';
        return 0;
    }
    
    size_t len = strlen(hostname_ptr);
    if (len >= max_len) {
        len = max_len - 1;  // Leave room for null terminator
    }
    
    memcpy(hostname, hostname_ptr, len);
    hostname[len] = '\0';
    
    return len;
}

/**
 * Get system description (defaults to "ESP32 EtherNet/IP Adapter")
 * @param description Buffer to store description (must be at least max_len bytes)
 * @param max_len Maximum length of description buffer
 * @return Length of description string
 */
size_t lldp_get_system_description(char *description, size_t max_len) {
    if (description == NULL || max_len == 0) {
        return 0;
    }
    
    // Default system description (shortened to avoid potential parsing issues)
    const char *default_desc = "ESP32-P4 ENIP Adapter";
    size_t len = strlen(default_desc);
    
    if (len >= max_len) {
        len = max_len - 1;  // Leave room for null terminator
    }
    
    memcpy(description, default_desc, len);
    description[len] = '\0';
    
    return len;
}

/**
 * Get IP address from network interface (network byte order)
 * @param ip_address Output buffer for IP address (4 bytes)
 * @return 0 on success, -1 if IP address not available
 */
int lldp_get_ip_address(uint8_t ip_address[4]) {
    if (ip_address == NULL) {
        return -1;
    }
    
    if (s_netif == NULL) {
        memset(ip_address, 0, 4);
        return -1;
    }
    
    // Check if IP address is valid (not 0.0.0.0)
    if (ip4_addr_get_u32(ip_2_ip4(&s_netif->ip_addr)) == 0) {
        memset(ip_address, 0, 4);
        return -1;
    }
    
    // Get IP address in network byte order (big-endian)
    // ip4_addr_get_u32() returns the address already in network byte order
    // We need to extract bytes directly from the memory representation,
    // not using bit shifts which depend on host byte order
    const ip4_addr_t *ip4 = ip_2_ip4(&s_netif->ip_addr);
    
    // Access bytes directly - lwIP stores IP addresses in network byte order (big-endian)
    // Use ip4_addr_get_byte() macros which handle byte order correctly
    ip_address[0] = ip4_addr_get_byte(ip4, 0);  // MSB (first octet)
    ip_address[1] = ip4_addr_get_byte(ip4, 1);  // Second octet
    ip_address[2] = ip4_addr_get_byte(ip4, 2);  // Third octet
    ip_address[3] = ip4_addr_get_byte(ip4, 3);  // LSB (fourth octet)
    
    return 0;
}

