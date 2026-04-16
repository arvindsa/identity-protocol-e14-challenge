/*
 * Stub: btstack_link_key_db_tlv_get_instance
 *
 * The MAX32630FTHR BTstack port (btstack_port.c) unconditionally calls
 * btstack_link_key_db_tlv_get_instance() during HCI init, even in BLE-only
 * builds.  The real implementation in btstack_link_key_db_tlv.c includes
 * classic/core.h which requires ENABLE_CLASSIC — and enabling that pulls in
 * SBC/A2DP headers that are not present in this tree.
 *
 * This stub returns NULL (no link-key storage) so the BLE-only beacon builds
 * and runs correctly.  Classic pairing is never initiated, so the NULL return
 * is safe.
 */

#include <stddef.h>
#include "classic/btstack_link_key_db_tlv.h"

const btstack_link_key_db_t *
btstack_link_key_db_tlv_get_instance(const btstack_tlv_t *btstack_tlv_impl,
                                     void               *btstack_tlv_context)
{
    (void)btstack_tlv_impl;
    (void)btstack_tlv_context;
    return NULL;
}

/* hci_set_link_key_db is defined in hci.c under #ifdef ENABLE_CLASSIC.
 * The port calls it unconditionally, so provide a no-op stub here. */
void hci_set_link_key_db(btstack_link_key_db_t const *link_key_db)
{
    (void)link_key_db;
}
