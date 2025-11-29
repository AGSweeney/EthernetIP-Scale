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

#include "ciplldpdatatable_instance.h"
#include "ciplldpdatatable.h"
#include "opener_api.h"
#include "cipcommon.h"
#include "ciperror.h"
#include "trace.h"
#include "cpf.h"
#include <string.h>
#include <stdlib.h>

// Forward declarations for encode functions
static void EncodeEthernetLinkInstanceNumber(const void *const data, ENIPMessage *const outgoing_message);
static void EncodeMacAddress(const void *const data, ENIPMessage *const outgoing_message);
static void EncodeInterfaceLabel(const void *const data, ENIPMessage *const outgoing_message);
static void EncodeTimeToLive(const void *const data, ENIPMessage *const outgoing_message);
static void EncodeSystemCapabilitiesTlv(const void *const data, ENIPMessage *const outgoing_message);
static void EncodeIpv4ManagementAddresses(const void *const data, ENIPMessage *const outgoing_message);
static void EncodeCipIdentification(const void *const data, ENIPMessage *const outgoing_message);
static void EncodeAdditionalEthernetCapabilities(const void *const data, ENIPMessage *const outgoing_message);
static void EncodeLastChange(const void *const data, ENIPMessage *const outgoing_message);
static void EncodePositionId(const void *const data, ENIPMessage *const outgoing_message);
static void EncodeT1sPhyConfiguration(const void *const data, ENIPMessage *const outgoing_message);

/**
 * Get the LLDP Data Table class
 */
static CipClass *GetLldpDataTableClass(void) {
    CipClass *class = GetCipClass(kCipLldpDataTableCode);
    if (class == NULL) {
        OPENER_TRACE_WARN("LLDP: Data Table class 0x%x not found\n", (unsigned)kCipLldpDataTableCode);
    }
    return class;
}

/**
 * Convert neighbor entry to CIP object values structure
 */
static void NeighborEntryToCipValues(const lldp_neighbor_entry_t *neighbor, 
                                     CipLldpDataTableObjectValues *values) {
    if (neighbor == NULL || values == NULL) {
        return;
    }
    
    // Initialize all fields
    memset(values, 0, sizeof(CipLldpDataTableObjectValues));
    
    // Attribute 1: Ethernet Link Instance Number (always 1 for single interface)
    values->ethernet_link_instance_number = 1;
    
    // Attribute 2: MAC Address - store in instance data structure
    memcpy(values->mac_address_storage, neighbor->source_mac, 6);
    values->mac_address = values->mac_address_storage;
    
    // Attribute 3: Interface Label (from Port ID if available)
    if (neighbor->port_id_len > 0 && neighbor->port_id_len < 256) {
        values->interface_label.length = (CipUsint)neighbor->port_id_len;
        // Store Port ID string in instance data
        memcpy(values->interface_label_storage, neighbor->port_id, neighbor->port_id_len);
        values->interface_label.string = values->interface_label_storage;
    } else {
        // Empty interface label
        values->interface_label.length = 0;
        values->interface_label.string = values->interface_label_storage;  // Still point to storage (even if empty)
        values->interface_label_storage[0] = 0;
    }
    
    // Attribute 4: Time To Live
    values->time_to_live = neighbor->ttl_seconds;
    
    // Attribute 5: System Capabilities TLV
    if (neighbor->has_system_capabilities) {
        values->system_capabilities_tlv.system_capabilities = neighbor->system_capabilities;
        values->system_capabilities_tlv.enabled_capabilities = neighbor->enabled_capabilities;
    } else {
        values->system_capabilities_tlv.system_capabilities = 0;
        values->system_capabilities_tlv.enabled_capabilities = 0;
    }
    
    // Attribute 6: IPv4 Management Addresses
    if (neighbor->has_management_ip) {
        values->ip_address_storage[0] = ((CipUdint)neighbor->management_ip[0] << 24) |
                                        ((CipUdint)neighbor->management_ip[1] << 16) |
                                        ((CipUdint)neighbor->management_ip[2] << 8) |
                                        ((CipUdint)neighbor->management_ip[3]);
        values->ipv4_management_addresses.management_address_count = 1;
        values->ipv4_management_addresses.management_addresses = values->ip_address_storage;
    } else {
        values->ipv4_management_addresses.management_address_count = 0;
        values->ipv4_management_addresses.management_addresses = NULL;
    }
    
    // Attribute 7: CIP Identification (not available from LLDP, set to zero)
    memset(&values->cip_identification, 0, sizeof(CipLldpDataTableCipIdentification));
    
    // Attribute 8: Additional Ethernet Capabilities (set to zero)
    memset(&values->additional_ethernet_capabilities, 0, sizeof(CipLldpDataTableAdditionalEthernetCapabilities));
    
    // Attribute 9: Last Change
    values->last_change = neighbor->last_update;
    
    // Attribute 10: Position ID (set to zero - not used)
    values->position_id = 0;
    
    // Attribute 11: T1S PHY Configuration (set to zero)
    memset(&values->t1s_phy_configuration, 0, sizeof(CipLldpDataTableT1sPhyConfiguration));
}

/**
 * Encode Attribute 1: Ethernet Link Instance Number
 */
static void EncodeEthernetLinkInstanceNumber(const void *const data, ENIPMessage *const outgoing_message) {
    const CipUint *value = (const CipUint *)data;
    AddIntToMessage(*value, outgoing_message);
}

/**
 * Encode Attribute 2: MAC Address (6 bytes)
 */
static void EncodeMacAddress(const void *const data, ENIPMessage *const outgoing_message) {
    const CipOctet *mac = *(const CipOctet **)data;
    if (mac != NULL) {
        for (int i = 0; i < 6; i++) {
            AddSintToMessage(mac[i], outgoing_message);
        }
    } else {
        // Zero MAC address
        for (int i = 0; i < 6; i++) {
            AddSintToMessage(0, outgoing_message);
        }
    }
}

/**
 * Encode Attribute 3: Interface Label (SHORT_STRING)
 */
static void EncodeInterfaceLabel(const void *const data, ENIPMessage *const outgoing_message) {
    const CipShortString *label = (const CipShortString *)data;
    AddSintToMessage(label->length, outgoing_message);
    for (CipUsint i = 0; i < label->length; i++) {
        AddSintToMessage(label->string[i], outgoing_message);
    }
}

/**
 * Encode Attribute 4: Time To Live
 */
static void EncodeTimeToLive(const void *const data, ENIPMessage *const outgoing_message) {
    const CipUint *value = (const CipUint *)data;
    AddIntToMessage(*value, outgoing_message);
}

/**
 * Encode Attribute 5: System Capabilities TLV
 */
static void EncodeSystemCapabilitiesTlv(const void *const data, ENIPMessage *const outgoing_message) {
    const CipLldpDataTableSystemCapabilitesTlv *tlv = (const CipLldpDataTableSystemCapabilitesTlv *)data;
    AddIntToMessage(tlv->system_capabilities, outgoing_message);
    AddIntToMessage(tlv->enabled_capabilities, outgoing_message);
}

/**
 * Encode Attribute 6: IPv4 Management Addresses
 */
static void EncodeIpv4ManagementAddresses(const void *const data, ENIPMessage *const outgoing_message) {
    const CipLldpDataTableIpv4ManagementAddresses *addrs = (const CipLldpDataTableIpv4ManagementAddresses *)data;
    AddSintToMessage(addrs->management_address_count, outgoing_message);
    for (CipUsint i = 0; i < addrs->management_address_count; i++) {
        AddDintToMessage(addrs->management_addresses[i], outgoing_message);
    }
}

/**
 * Encode Attribute 7: CIP Identification
 */
static void EncodeCipIdentification(const void *const data, ENIPMessage *const outgoing_message) {
    const CipLldpDataTableCipIdentification *ident = (const CipLldpDataTableCipIdentification *)data;
    AddIntToMessage(ident->vendor_id, outgoing_message);
    AddIntToMessage(ident->device_type, outgoing_message);
    AddIntToMessage(ident->product_code, outgoing_message);
    AddSintToMessage(ident->major_revision, outgoing_message);
    AddSintToMessage(ident->minor_revision, outgoing_message);
    AddDintToMessage(ident->cip_serial_number, outgoing_message);
}

/**
 * Encode Attribute 8: Additional Ethernet Capabilities
 */
static void EncodeAdditionalEthernetCapabilities(const void *const data, ENIPMessage *const outgoing_message) {
    const CipLldpDataTableAdditionalEthernetCapabilities *caps = (const CipLldpDataTableAdditionalEthernetCapabilities *)data;
    AddSintToMessage(caps->preemption_support ? 1 : 0, outgoing_message);
    AddSintToMessage(caps->preemption_status ? 1 : 0, outgoing_message);
    AddSintToMessage(caps->preemption_active ? 1 : 0, outgoing_message);
    AddSintToMessage(caps->additional_fragment_size, outgoing_message);
}

/**
 * Encode Attribute 9: Last Change
 */
static void EncodeLastChange(const void *const data, ENIPMessage *const outgoing_message) {
    const CipUdint *value = (const CipUdint *)data;
    AddDintToMessage(*value, outgoing_message);
}

/**
 * Encode Attribute 10: Position ID
 */
static void EncodePositionId(const void *const data, ENIPMessage *const outgoing_message) {
    const CipUint *value = (const CipUint *)data;
    AddIntToMessage(*value, outgoing_message);
}

/**
 * Encode Attribute 11: T1S PHY Configuration
 */
static void EncodeT1sPhyConfiguration(const void *const data, ENIPMessage *const outgoing_message) {
    const CipLldpDataTableT1sPhyConfiguration *phy = (const CipLldpDataTableT1sPhyConfiguration *)data;
    AddSintToMessage(phy->phy_mode, outgoing_message);
    AddSintToMessage(phy->phy_node_id_for_pcla, outgoing_message);
}

/**
 * Get next available instance number
 */
CipInstanceNum lldp_datatable_get_next_instance_number(void) {
    CipClass *class = GetLldpDataTableClass();
    if (class == NULL) {
        return 0;
    }
    
    // Find highest instance number
    CipInstanceNum max_instance = 0;
    CipInstance *instance = class->instances;
    while (instance != NULL) {
        if (instance->instance_number > max_instance) {
            max_instance = instance->instance_number;
        }
        instance = instance->next;
    }
    
    return max_instance + 1;
}

/**
 * Create a CIP object instance for a neighbor entry
 */
int lldp_datatable_create_instance(lldp_neighbor_entry_t *neighbor_entry, CipInstanceNum instance_number) {
    if (neighbor_entry == NULL) {
        return -1;
    }
    
    CipClass *class = GetLldpDataTableClass();
    if (class == NULL) {
        OPENER_TRACE_ERR("LLDP Data Table class not found\n");
        return -1;
    }
    
    // Auto-assign instance number if not specified
    if (instance_number == 0) {
        instance_number = lldp_datatable_get_next_instance_number();
    }
    
    // Check if instance already exists
    CipInstance *existing = GetCipInstance(class, instance_number);
    if (existing != NULL) {
        // Update existing instance
        return lldp_datatable_update_instance(neighbor_entry);
    }
    
    // Allocate instance data structure
    CipLldpDataTableObjectValues *instance_data = (CipLldpDataTableObjectValues *)CipCalloc(1, sizeof(CipLldpDataTableObjectValues));
    if (instance_data == NULL) {
        OPENER_TRACE_ERR("Failed to allocate instance data\n");
        return -1;
    }
    
    // Convert neighbor entry to CIP values
    NeighborEntryToCipValues(neighbor_entry, instance_data);
    
    // Create CIP instance
    CipInstance *instance = AddCipInstance(class, instance_number);
    if (instance == NULL) {
        OPENER_TRACE_ERR("Failed to create CIP instance\n");
        CipFree(instance_data);
        return -1;
    }
    
    // Data Table instance created - no logging needed (Neighbor discovered log covers this)
    
    // Store instance data pointer
    instance->data = instance_data;
    
    // Register all attributes
    InsertAttribute(instance, 1, kCipUint, EncodeEthernetLinkInstanceNumber, NULL,
                    &instance_data->ethernet_link_instance_number, kGetableSingleAndAll);
    InsertAttribute(instance, 2, kCipAny, EncodeMacAddress, NULL,
                    &instance_data->mac_address, kGetableSingleAndAll);
    InsertAttribute(instance, 3, kCipShortString, EncodeInterfaceLabel, NULL,
                    &instance_data->interface_label, kGetableSingleAndAll);
    InsertAttribute(instance, 4, kCipUint, EncodeTimeToLive, NULL,
                    &instance_data->time_to_live, kGetableSingleAndAll);
    InsertAttribute(instance, 5, kCipAny, EncodeSystemCapabilitiesTlv, NULL,
                    &instance_data->system_capabilities_tlv, kGetableSingleAndAll);
    InsertAttribute(instance, 6, kCipAny, EncodeIpv4ManagementAddresses, NULL,
                    &instance_data->ipv4_management_addresses, kGetableSingleAndAll);
    InsertAttribute(instance, 7, kCipAny, EncodeCipIdentification, NULL,
                    &instance_data->cip_identification, kGetableSingleAndAll);
    InsertAttribute(instance, 8, kCipAny, EncodeAdditionalEthernetCapabilities, NULL,
                    &instance_data->additional_ethernet_capabilities, kGetableSingleAndAll);
    InsertAttribute(instance, 9, kCipUdint, EncodeLastChange, NULL,
                    &instance_data->last_change, kGetableSingleAndAll);
    InsertAttribute(instance, 10, kCipUint, EncodePositionId, NULL,
                    &instance_data->position_id, kGetableSingleAndAll);
    InsertAttribute(instance, 11, kCipAny, EncodeT1sPhyConfiguration, NULL,
                    &instance_data->t1s_phy_configuration, kGetableSingleAndAll);
    
    // Store reference in neighbor entry
    neighbor_entry->cip_instance_ptr = instance;
    neighbor_entry->instance_number = instance_number;
    
    // Instance fully registered - no logging needed (Neighbor discovered log covers this)
    
    return 0;
}

/**
 * Update a CIP object instance with current neighbor data
 */
int lldp_datatable_update_instance(lldp_neighbor_entry_t *neighbor_entry) {
    if (neighbor_entry == NULL || neighbor_entry->cip_instance_ptr == NULL) {
        return -1;
    }
    
    CipInstance *instance = (CipInstance *)neighbor_entry->cip_instance_ptr;
    CipLldpDataTableObjectValues *instance_data = (CipLldpDataTableObjectValues *)instance->data;
    
    if (instance_data == NULL) {
        return -1;
    }
    
    // Update values from neighbor entry
    NeighborEntryToCipValues(neighbor_entry, instance_data);
    
    return 0;
}

/**
 * Delete a CIP object instance for a neighbor entry
 */
int lldp_datatable_delete_instance(lldp_neighbor_entry_t *neighbor_entry) {
    if (neighbor_entry == NULL || neighbor_entry->cip_instance_ptr == NULL) {
        return -1;
    }
    
    CipClass *class = GetLldpDataTableClass();
    if (class == NULL) {
        return -1;
    }
    
    CipInstance *instance = (CipInstance *)neighbor_entry->cip_instance_ptr;
    
    // Free instance data
    if (instance->data != NULL) {
        CipFree(instance->data);
        instance->data = NULL;
    }
    
    // Remove from class instance list
    CipInstance *prev = NULL;
    CipInstance *current = class->instances;
    
    while (current != NULL) {
        if (current == instance) {
            if (prev == NULL) {
                class->instances = current->next;
            } else {
                prev->next = current->next;
            }
            
            // Free instance
            if (class->number_of_attributes > 0 && current->attributes != NULL) {
                CipFree(current->attributes);
            }
            CipFree(current);
            
            class->number_of_instances--;
            class->max_instance = GetMaxInstanceNumber(class);
            
            // Clear neighbor entry reference
            neighbor_entry->cip_instance_ptr = NULL;
            neighbor_entry->instance_number = 0;
            
            return 0;
        }
        prev = current;
        current = current->next;
    }
    
    return -1;
}

