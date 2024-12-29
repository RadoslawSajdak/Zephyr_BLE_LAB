/* ============================================================================================== */
/*                                            INCLUDES                                            */
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <dk_buttons_and_leds.h>

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/bluetooth/gatt.h>

/* ============================================================================================== */
/*                                        DEFINES/TYPEDEFS                                        */
#define ZEPHYR_BLE_GATT_DATA_SVC_UUID16         (0xDEAD)
#define ZEPHYR_DATA_SVC_UUID                    0xFB, 0x34, 0x9B, 0x5F,\
                                                0x80, 0x00, 0x00, 0x80,\
                                                0x00, 0x10, 0x00, 0x00,\
                                                (ZEPHYR_BLE_GATT_DATA_SVC_UUID16 & 0xFF),\
                                                (ZEPHYR_BLE_GATT_DATA_SVC_UUID16 >> 8), 0x00, 0x00

#define ZEPHYR_BLE_GATT_DATA_CHRC_UUID16        (0xBEEF)
#define ZEPHYR_DATA_CHRC_UUID                   0xFB, 0x34, 0x9B, 0x5F,\
                                                0x80, 0x00, 0x00, 0x80,\
                                                0x00, 0x10, 0x00, 0x00,\
                                                (ZEPHYR_BLE_GATT_DATA_CHRC_UUID16 & (0xFF)),\
                                                (ZEPHYR_BLE_GATT_DATA_CHRC_UUID16 >> 8), 0x00, 0x00

#define BT_UUID_ZEPHYR_SVC                      BT_UUID_DECLARE_128(ZEPHYR_DATA_SVC_UUID)
#define BT_UUID_ZEPHYR_DATA                     BT_UUID_DECLARE_128(ZEPHYR_DATA_CHRC_UUID)

typedef struct __packed
{
    uint16_t vendor_id;
    uint16_t press_count;
} manufacturer_data_adv_payload_t;
/* ============================================================================================== */
/*                                   PRIVATE FUNCTION DEFINITIONS                                 */
static int  bt_adv_init(void);
static int  adv_update(void);
static void connected(struct bt_conn *conn, uint8_t err);
static void disconnected(struct bt_conn *conn, uint8_t reason);
static void button_changed_callback(uint32_t button_state, uint32_t has_changed);

static ssize_t data_rx(struct bt_conn *conn, const struct bt_gatt_attr *attr, const void *buf,
                         uint16_t len, uint16_t offset, uint8_t flags);
static ssize_t data_tx(struct bt_conn *conn, const struct bt_gatt_attr *attr, void *buf,
                         uint16_t len, uint16_t offset);

/* ============================================================================================== */
/*                                        PRIVATE VARIABLES                                       */
static manufacturer_data_adv_payload_t manuf_data_payload = {
    .vendor_id = 0x0059,
    .press_count = 0
};

static const struct bt_data ad[] = {
    BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
    BT_DATA_BYTES(BT_DATA_UUID16_SOME, BT_UUID_16_ENCODE(ZEPHYR_BLE_GATT_DATA_SVC_UUID16)),
    BT_DATA(BT_DATA_MANUFACTURER_DATA, (uint8_t *) &manuf_data_payload, 
            sizeof(manufacturer_data_adv_payload_t))
};

BT_GATT_SERVICE_DEFINE(bt_service, 
    BT_GATT_PRIMARY_SERVICE(BT_UUID_ZEPHYR_SVC),
        BT_GATT_CHARACTERISTIC( BT_UUID_ZEPHYR_DATA,
                                BT_GATT_CHRC_WRITE_WITHOUT_RESP | BT_GATT_CHRC_READ,
                                   BT_GATT_PERM_WRITE | BT_GATT_PERM_READ,
                                   data_tx, data_rx, NULL)
);
static struct bt_le_ext_adv *adv;

BT_CONN_CB_DEFINE(conn_callbacks) = {
    .connected = connected,
    .disconnected = disconnected
};

K_MSGQ_DEFINE(bt_msgq, sizeof(uint8_t), 16U, 1U);
static ATOMIC_DEFINE(is_connected, 1U);
/* ============================================================================================== */
/*                                        PUBLIC FUNCTIONS                                        */

int main(void)
{
    int err;

    err = dk_buttons_init(button_changed_callback);
    if (err)
    {
        printk("Failed to initialize buttons (err %d)\n", err);
        return err;
    }

    err = dk_leds_init();
    if (err) {
        printk("Failed to initialize LEDs (err %d)\n", err);
        return err;
    }

    err = bt_enable(NULL);
    if (err)
    {
        printk("Bluetooth init failed (err %d)\n", err);
        return err;
    }

    printk("Bluetooth initialized\n");
    bt_adv_init();
    printk("Advertising successfully started\n");


    /* Implement notification. */
    while (1)
    {
        //TODO: Flash led sequence after disconnect using:
        // - atomic_get()
        // - k_msgq_get()
        // - dk_set_led()
        k_msleep(100);
    }

    return 0;
}

/* ============================================================================================== */
/*                                        PRIVATE FUNCTIONS                                       */
static void connected(struct bt_conn *conn, uint8_t err)
{
    if (err)
        printk("Connection failed (err 0x%02x)\n", err);
    else
    {
        printk("Connected\n");
        (void)atomic_set(is_connected, true);
    }
}

static void disconnected(struct bt_conn *conn, uint8_t reason)
{
    printk("Disconnected (reason 0x%02x)\n", reason);
    (void)atomic_set(is_connected, false);

    //TODO: User can connect to the device only once
}

static void button_changed_callback(uint32_t button_state, uint32_t has_changed)
{
    // TODO: Increment the press_count on falling edge
    // Hint DK_BTN1_MSK
    if (0)
    {
    }
}


static int bt_adv_init(void)
{
    int err;

    struct bt_le_adv_param adv_param = {
        .id = BT_ID_DEFAULT,
        .options = (BT_LE_ADV_OPT_CONNECTABLE |
                    BT_LE_ADV_OPT_USE_NAME | 
                    BT_LE_ADV_OPT_USE_IDENTITY),
        .interval_min = BT_GAP_ADV_FAST_INT_MIN_2,
        .interval_max = BT_GAP_ADV_FAST_INT_MAX_2,
    };

    err = bt_le_ext_adv_create(&adv_param, NULL, &adv);
    if (err) {
        printk("Failed to create extended advertising set (err %d)\n", err);
        return err;
    }

    printk("Setting extended advertising data\n");
    err = bt_le_ext_adv_set_data(adv, ad, ARRAY_SIZE(ad), NULL, 0);
    if (err) {
        printk("Failed to set extended advertising data (err %d)\n", err);
        return err;
    }


    printk("Starting Extended Advertising\n");
    err = bt_le_ext_adv_start(adv, BT_LE_EXT_ADV_START_DEFAULT);
    if (err) {
        printk("Failed to start extended advertising set (err %d)\n", err);
        return 0;
    }
    return 0;
}

static int adv_update(void)
{
    return bt_le_ext_adv_set_data(adv, ad, ARRAY_SIZE(ad), NULL, 0);
}

static ssize_t data_rx(struct bt_conn *conn, const struct bt_gatt_attr *attr, const void *buf,
                         uint16_t len, uint16_t offset, uint8_t flags)
{
    printk("Writing characteristic %#04x\n", ZEPHYR_BLE_GATT_DATA_CHRC_UUID16);
    if (len > k_msgq_num_free_get(&bt_msgq))
        return -ENOMEM;

    for (int i = 0; i < len; i++)
    {
        k_msgq_put(&bt_msgq, ((uint8_t *)buf + i), K_NO_WAIT);
    }
    return 0;
}

static ssize_t data_tx(struct bt_conn *conn, const struct bt_gatt_attr *attr, void *buf,
                         uint16_t len, uint16_t offset)
{
    printk("Reading characteristic %#04x\n", ZEPHYR_BLE_GATT_DATA_CHRC_UUID16);
    // TODO: Read current number of presses
    return bt_gatt_attr_read(conn, attr, (void *)buf, (uint16_t)len, offset, NULL, 0);
}

