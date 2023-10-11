/** \file   joystick.c
 * \brief   joystick interface using libevdev
 * \author  Bas Wassink <b.wassink@ziggo.nl>
 *
 * Button, axis and hat names taken from:
 * https://github.com/torvalds/linux/blob/master/drivers/hid/hid-debug.c
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>
#include <limits.h>

#include <dirent.h>
#include <fcntl.h>
#include <libevdev/libevdev.h>
#include <linux/input.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "vice.h"

#include "joystick.h"


#define ARRAY_LEN(arr)  (sizeof arr / sizeof arr[0])

#define JOY_UDEV_SUFFIX "-event-joystick"

typedef struct ev_code_name_s {
    unsigned int  code;
    const char   *name;
} ev_code_name_t;


/* The XBox "profile" axis is a recent addition, from kernel ~6.1 onward, so we
 * define it ourselves here.
 * See https://git.kernel.org/pub/scm/linux/kernel/git/stable/linux.git/commit/?h=v6.1.56&id=1260cd04a601e0e02e09fa332111b8639611970d
 */
#ifndef ABS_PROFILE
#define ABS_PROFILE 0x21
#endif

/** \brief  Button names */
static const ev_code_name_t button_names[] = {
    /* 0x100-0x109 - BTN_MISC */
    { BTN_0,        "Btn0" },           { BTN_1,        "Btn1" },
    { BTN_0,        "Btn2" },           { BTN_1,        "Btn3" },
    { BTN_0,        "Btn4" },           { BTN_1,        "Btn5" },
    { BTN_0,        "Btn6" },           { BTN_1,        "Btn7" },
    { BTN_0,        "Btn8" },           { BTN_1,        "Btn9" },

    /* 0x110-0x117 - BTN_MOUSE */
    { BTN_LEFT,     "LeftBtn" },        { BTN_RIGHT,    "RightBtn" },
    { BTN_MIDDLE,   "MiddleBtn" },      { BTN_SIDE,     "SideBtn" },
    { BTN_EXTRA,    "ExtraBtn" },       { BTN_FORWARD,  "FowardBtn" },
    { BTN_BACK,     "BackBtn" },        { BTN_TASK,     "TaskBtn" },

    /* 0x120-0x12f - BTN_JOYSTICK */
    { BTN_TRIGGER,  "Trigger" },        { BTN_THUMB,    "ThumbBtn" },
    { BTN_THUMB2,   "ThumbBtn2" },      { BTN_TOP,      "TopBtn" },
    { BTN_TOP2,     "TopBtn2" },        { BTN_PINKIE,   "PinkieButton" },
    { BTN_BASE,     "BaseBtn" },        { BTN_BASE2,    "BaseBtn2" },
    { BTN_BASE3,    "BaseBtn3" },       { BTN_BASE4,    "BaseBtn4" },
    { BTN_BASE5,    "BaseBtn5" },       { BTN_BASE6,    "BaseBtn6" },
    { BTN_DEAD,     "BtnDead" },

    /* 0x130-0x13e - BTN_GAMEPAD */
    { BTN_A,        "BtnA" },           { BTN_B,        "BtnB" },
    { BTN_C,        "BtnC" },           { BTN_X,        "BtnX" },
    { BTN_Y,        "BtnY" },           { BTN_Z,        "BtnZ" },
    { BTN_TL,       "BtnTL" },          { BTN_TR,       "BtnTR" },
    { BTN_TL2,      "BtnTL2" },         { BTN_TR2,      "BtnTR2" },
    { BTN_SELECT,   "BtnSelect" },      { BTN_START,    "BtnStart" },
    { BTN_MODE,     "BtnMode" },        { BTN_THUMBL,   "BtnThumbL" },
    { BTN_THUMBR,   "BtnThumbR" },

    /* Skipping 0x140-0x14f: BTN_TOOL_PEN - BTN_TOOL_QUADTAP */

    /* 0x150-0x151: BTN_WHEEL */
    { BTN_GEAR_DOWN, "GearDown" },      { BTN_GEAR_UP,  "GearUp" },

    /* 0x220-0223 */
    { BTN_DPAD_UP,   "BtnDPadUp" },     { BTN_DPAD_DOWN,  "BtnDPadDown" },
    { BTN_DPAD_LEFT, "BtnDPadLeft" },   { BTN_DPAD_RIGHT, "BtnDPadRight" }
};

/** \brief  Axis names */
static const ev_code_name_t axis_names[] = {
    { ABS_X,            "X" },
    { ABS_Y,            "Y" },
    { ABS_Z,            "Z" },
    { ABS_RX,           "Rx" },
    { ABS_RY,           "Ry" },
    { ABS_RZ,           "Rz" },
    { ABS_THROTTLE,     "Throttle" },
    { ABS_RUDDER,       "Rudder" },
    { ABS_WHEEL,        "Wheel" },
    { ABS_GAS,          "Gas" },
    { ABS_BRAKE,        "Brake" },
    { ABS_HAT0X,        "Hat0X" },
    { ABS_HAT0Y,        "Hat0Y" },
    { ABS_HAT1X,        "Hat1X" },
    { ABS_HAT1Y,        "Hat1Y" },
    { ABS_HAT2X,        "Hat2X" },
    { ABS_HAT2Y,        "Hat2Y" },
    { ABS_HAT3X,        "Hat3X" },
    { ABS_HAT3Y,        "Hat3Y" },
    { ABS_PRESSURE,     "Pressure" },
    { ABS_DISTANCE,     "Distance" },
    { ABS_TILT_X,       "XTilt" },
    { ABS_TILT_Y,       "YTilt" },
    { ABS_TOOL_WIDTH,   "ToolWidth" },
    { ABS_VOLUME,       "Volume" },
    { ABS_PROFILE,      "Profile" },
    { ABS_MISC,         "Misc" }
};

/** \brief  Hat names
 *
 * Each of two hat axes maps to the same name.
 */
static const ev_code_name_t hat_names[] = {
    { ABS_HAT0X,    "Hat0" },
    { ABS_HAT0Y,    "Hat0" },
    { ABS_HAT1X,    "Hat1" },
    { ABS_HAT1Y,    "Hat1" },
    { ABS_HAT2X,    "Hat2" },
    { ABS_HAT2Y,    "Hat2" },
    { ABS_HAT3X,    "Hat3" },
    { ABS_HAT3Y,    "Hat3" },
};



/** \brief  Get axis name for event code
 *
 * \param[in]   code    event code
 *
 * \return  name of axis
 */
const char *joy_get_axis_name(unsigned int code)
{
    size_t i;

    for (i = 0; i < ARRAY_LEN(axis_names); i++) {
        if (axis_names[i].code == code) {
            return axis_names[i].name;
        } else if (axis_names[i].code > code) {
            break;
        }
    }
    return "<?>";
}


/** \brief  Get button name for event code
 *
 * \param[in]   code    event code
 *
 * \return  name of button
 */
const char *joy_get_button_name(unsigned int code)
{
    size_t i;

    for (i = 0; i < ARRAY_LEN(button_names); i++) {
        if (button_names[i].code == code) {
            return button_names[i].name;
        } else if (button_names[i].code > code) {
            break;
        }
    }
    return "<?>";
}


/** \brief  Get hat name for event code
 *
 * \param[in]   code    event code
 *
 * \return  name of hat
 */
const char *joy_get_hat_name(unsigned int code)
{
    size_t i ;

    for (i = 0; i < ARRAY_LEN(hat_names); i++) {
        if (hat_names[i].code == code) {
            return hat_names[i].name;
        } else if (hat_names[i].code > code) {
            break;
        }
    }
    return "<?>";
}


/** \brief  Clear joystick info struct
 *
 * \param[in]   info    joystick info
 */
static void dev_info_clear(joy_dev_info_t *info)
{
    info->path        = NULL;
    info->name        = NULL;
    memset(info->guid, 0, sizeof info->guid);
    memset(info->guid_str, 0, sizeof info->guid_str);

    info->bustype     = 0;
    info->vendor      = 0;
    info->product     = 0;
    info->version     = 0;

    info->num_axes    = 0;
    info->num_buttons = 0;
    info->num_hats    = 0;
    info->num_balls   = 0;

    info->button_map  = NULL;
    info->axis_map    = NULL;
    info->hat_map     = NULL;
}

/** \brief  Free memory used by members of a joystick info struct
 *
 * Does not free \a info itself.
 *
 * \param[in]   info    joystick info
 */
static void dev_info_free_data(joy_dev_info_t *info)
{
    lib_free(info->path);
    lib_free(info->name);
    lib_free(info->button_map);
    lib_free(info->axis_map);
    lib_free(info->hat_map);
}

/** \brief  Generate GUID in the format used by SDL's mapping files
 *
 * Generate GUID from bus type, vendor, product and version.
 *
 * \param[in]  info joystick info
 */
static void dev_info_generate_guid(joy_dev_info_t *info)
{
    uint16_t bus = info->bustype;
    uint16_t ven = info->vendor;
    uint16_t pro = info->product;
    uint16_t ver = info->version;

    info->guid[0x00] = (uint8_t)(bus & 0xff);
    info->guid[0x01] = (uint8_t)(bus >> 8);
    /* SDL stores CRC16 of a "description" string here */
    info->guid[0x02] = 0;
    info->guid[0x03] = 0;
    info->guid[0x04] = (uint8_t)(ven & 0xff);
    info->guid[0x05] = (uint8_t)(ven >> 8);
    info->guid[0x06] = 0;
    info->guid[0x07] = 0;
    info->guid[0x08] = (uint8_t)(pro & 0xff);
    info->guid[0x09] = (uint8_t)(pro >> 8);
    info->guid[0x0a] = 0;
    info->guid[0x0b] = 0;
    info->guid[0x0c] = (uint8_t)(ver & 0xff);
    info->guid[0x0d] = (uint8_t)(ver >> 8);
    /* SDL stores `driver_signature` here */
    info->guid[0x0e] = 0;
    /* SDL stores `driver_data` here */
    info->guid[0x0f] = 0;
}

/** \brief  Generate GUID string in the format used by SDL's mapping files
 *
 * Generate GUID string from bus type, vendor, product and version. This
 * requires first calling dev_info_generate_guid().
 *
 * \param[in]  info joystick info
 */
static void dev_info_generate_guid_str(joy_dev_info_t *info)
{
    static char digits[] = "0123456789abcdef";
    size_t i;

    for (i = 0; i < sizeof info->guid / sizeof info->guid[0]; i++) {
        info->guid_str[i * 2 + 0] = digits[info->guid[i] >> 8];
        info->guid_str[i * 2 + 1] = digits[info->guid[i] & 0x0f];
    }
    info->guid_str[i * 2] = '\0';
}

/** \brief  Scan joystick device for buttons present
 *
 * \param[in]   info    joystick info
 * \param[in]   dev     evdev instance
 */
static void dev_info_scan_buttons(joy_dev_info_t *info, struct libevdev *dev)
{
    if (libevdev_has_event_type(dev, EV_KEY)) {
        unsigned int num_buttons = 0;
        unsigned int code;

#if 0
        printf("<debug> device has event type EV_KEY\n");
#endif
        /* count number of buttons */
        for (code = BTN_MISC; code < KEY_MAX; code++) {
            if (libevdev_has_event_code(dev, EV_KEY, code)) {
                num_buttons++;
            }
        }
#if 0
        printf("<debug> %u buttons\n", num_buttons);
#endif
        info->num_buttons = (uint16_t)num_buttons;

        num_buttons = 0;
        info->button_map = lib_malloc(num_buttons * sizeof *(info->button_map));
        for (code = BTN_JOYSTICK; code < KEY_MAX; code++) {
            if (libevdev_has_event_code(dev, EV_KEY, code)) {
                info->button_map[num_buttons++] = (uint16_t)code;
#if 0
                printf("<debug> button %02x '%s'\n",
                       code, libevdev_event_code_get_name(EV_KEY, code));
#endif
            }
        }
    }
}

static bool is_hat_code(unsigned int code)
{
    return (bool)(code >= ABS_HAT0X && code <= ABS_HAT3Y);
}

static void dev_info_scan_axes_and_hats(joy_dev_info_t *info, struct libevdev *dev)
{
    if (libevdev_has_event_type(dev, EV_ABS)) {
        unsigned int num_axes = 0;
        unsigned int num_hats = 0;  /* number of hat *axes* */
        unsigned int code;
#if 0
        printf("<debug> device has event type EV_ABS\n");
#endif
        for (code = ABS_X; code < ABS_RESERVED; code++) {
            if (libevdev_has_event_code(dev, EV_ABS, code)) {
                if (is_hat_code(code)) {
                    num_hats++;
                } else {
                    num_axes++;
                }
            }
        }
#if 0
        printf("<debug> %u axes, %u hats\n", num_axes, num_hats / 2u);
#endif
        info->num_axes = (uint16_t)num_axes;
        info->num_hats = (uint16_t)num_hats / 2u;

        info->axis_map = lib_calloc(num_axes, sizeof *(info->axis_map));
        info->hat_map  = lib_calloc(num_hats, sizeof *(info->hat_map));

        num_axes = 0;
        num_hats = 0;
        for (code = ABS_X; code < ABS_RESERVED; code++) {
            if (libevdev_has_event_code(dev, EV_ABS, code)) {
                if (is_hat_code(code)) {
#if 0
                    printf("<debug> hat axis %u: %04x %s\n",
                           num_hats, code, libevdev_event_code_get_name(EV_ABS, code));
#endif
                    info->hat_map[num_hats++] = (uint16_t)code;
                } else {
#if 0
                    printf("<debug> axis %u: %04x %s\n",
                           num_axes, code, libevdev_event_code_get_name(EV_ABS, code));
#endif
                    info->axis_map[num_axes++] = (uint16_t)code;
                }
            }
        }
    }
}


static void iter_clear(joy_dev_iter_t *iter)
{
    iter->dirp  = NULL;
    iter->root  = NULL;
    iter->valid = false;
    dev_info_clear(&(iter->device));
}

static bool iter_get_entry(joy_dev_iter_t *iter)
{
    struct dirent *entry;
    size_t         len;

    /* clear previous device data */
    lib_free(iter->device.path);
    lib_free(iter->device.name);
    dev_info_clear(&(iter->device));

    errno = 0;
    entry = readdir(iter->dirp);
    if (entry == NULL) {
        return false;
    }

    len = strlen(entry->d_name);
    iter->device.path = lib_malloc(iter->root_len + 1u + len + 1u);
    memcpy(iter->device.path, iter->root, iter->root_len);
    if (iter->root[iter->root_len - 1u] != '/') {
        iter->device.path[iter->root_len] = '/';
        memcpy(iter->device.path + iter->root_len + 1, entry->d_name, len + 1u);
    } else {
        memcpy(iter->device.path + iter->root_len, entry->d_name, len + 1u);
    }
    return true;
}


static bool iter_has_evdev_joystick(joy_dev_iter_t *iter)
{
    struct stat stbuf;

    if (stat(iter->device.path, &stbuf) == 0) {
        if (!(stbuf.st_mode & S_IFDIR)) {
            size_t nlen = strlen(iter->device.path);
            size_t slen = strlen(JOY_UDEV_SUFFIX);

            if (nlen > slen &&
                    strcmp(iter->device.path + (nlen - slen), JOY_UDEV_SUFFIX) == 0) {
                return true;
            }
        }
    }
    return false;
}



static bool iter_get_dev_info(joy_dev_iter_t *iter)
{
    struct libevdev *dev;
    int              fd;
    int              rc;

    fd = open(iter->device.path, O_RDONLY|O_NONBLOCK);
    if (fd < 0) {
        fprintf(stderr,
                "error: failed to open %s: %s\n",
                iter->device.path, strerror(errno));
        return false;
    }
    rc = libevdev_new_from_fd(fd, &dev);
    if (rc < 0) {
        fprintf(stderr, "failed to initialize evdev: %s\n", strerror(-rc));
        close(fd);
        return false;
    }

    /* get device name */
    iter->device.name = lib_strdup(libevdev_get_name(dev));

    /* get bus, vendor, product and version */
    iter->device.bustype = (uint16_t)libevdev_get_id_bustype(dev);
    iter->device.vendor  = (uint16_t)libevdev_get_id_vendor(dev);
    iter->device.product = (uint16_t)libevdev_get_id_product(dev);
    iter->device.version = (uint16_t)libevdev_get_id_version(dev);

    /* generate 128 bit GUID and string version thereof */
    dev_info_generate_guid(&(iter->device));
    dev_info_generate_guid_str(&(iter->device));

    dev_info_scan_buttons(&(iter->device), dev);
    dev_info_scan_axes_and_hats(&(iter->device), dev);

    libevdev_free(dev);
    close(fd);
    return true;
}



bool joy_dev_iter_init(joy_dev_iter_t *iter, const char *path)
{
    iter_clear(iter);
    iter->dirp = opendir(path);
    if (iter->dirp == NULL) {
        fprintf(stderr, "failed to open %s: %s\n", path, strerror(errno));
        return false;
    } else {
        iter->root_len = strlen(path);
        iter->root     = lib_malloc(iter->root_len + 1u);
        memcpy(iter->root, path, iter->root_len + 1u);

        while (true) {
            if (!iter_get_entry(iter)) {
                closedir(iter->dirp);
                lib_free(iter->root);
                return false;
            }
            if (iter_has_evdev_joystick(iter)) {
                iter_get_dev_info(iter);
                break;
            }
        }
        iter->valid = true;
        return true;
    }
}


void joy_dev_iter_free(joy_dev_iter_t *iter)
{
    lib_free(iter->root);
    if (iter->dirp != NULL) {
        closedir(iter->dirp);
    }
    dev_info_free_data(&(iter->device));
    dev_info_clear(&(iter->device));
    iter_clear(iter);
}



bool joy_dev_iter_next(joy_dev_iter_t *iter)
{
    if (!iter->valid) {
        return false;
    }
    /* get next device node, skipping directories */
    while (true) {
        struct stat st;

        iter->valid = iter_get_entry(iter);
        if (!iter->valid) {
            return false;
        }
        if (stat(iter->device.path, &st) != 0) {
            fprintf(stderr, "stat() error: %s\n", strerror(errno));
            iter->valid = false;
            return false;
        }
        if (iter_has_evdev_joystick(iter)) {
            if (iter_get_dev_info(iter)) {
                return true;
            }
        }
    }
    return false;
}


joy_dev_info_t *joy_dev_iter_get_device_info(joy_dev_iter_t *iter)
{
    return iter->valid? &(iter->device) : NULL;
}


joy_dev_info_t *joy_dev_info_dup(const joy_dev_info_t *device)
{
    unsigned int    i;
    joy_dev_info_t *newdev;

    newdev = lib_malloc(sizeof *newdev);
    dev_info_clear(newdev);

    newdev->path = lib_strdup(device->path);
    newdev->name = lib_strdup(device->name);
    memcpy(newdev->guid, device->guid, sizeof device->guid);
    memcpy(newdev->guid_str, device->guid_str, sizeof device->guid_str);

    newdev->bustype     = device->bustype;
    newdev->vendor      = device->vendor;
    newdev->product     = device->product;
    newdev->version     = device->version;

    newdev->num_axes    = device->num_axes;
    newdev->num_buttons = device->num_buttons;
    newdev->num_hats    = device->num_hats;
    newdev->num_balls   = device->num_balls;

    if (device->num_buttons > 0) {
        newdev->button_map = lib_malloc(device->num_buttons * sizeof *(newdev->button_map));
        for (i = 0; i < device->num_buttons; i++) {
            newdev->button_map[i] = device->button_map[i];
        }
    }
    if (device->num_axes > 0) {
        newdev->axis_map = lib_malloc(device->num_axes * sizeof *(newdev->axis_map));
        for (i = 0; i < device->num_axes; i++) {
            newdev->axis_map[i] = device->axis_map[i];
        }
    }
    if (device->num_hats > 0) {
        newdev->hat_map = lib_malloc(device->num_hats * sizeof *(newdev->hat_map));
        for (i = 0; i < device->num_hats * 2u; i++) {
            newdev->hat_map[i] = device->hat_map[i];
        }
    }
    return newdev;
}


/** \brief  Free memory used by a joystick info struct
 *
 * \param[in]   info    joystick info
 */
void joy_dev_info_free(joy_dev_info_t *device)
{
    dev_info_free_data(device);
    lib_free(device);
}
