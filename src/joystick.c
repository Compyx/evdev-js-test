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


static joy_dev_info_t **devices_list;
static int              devices_count;


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

/** \brief  Clear info on an absolute event
 *
 * \param[in]   info    absolute event object
 */
static void abs_info_clear(joy_abs_info_t *info)
{
    info->code       = 0;
    info->minimum    = 0;
    info->maximum    = 0;
    info->fuzz       = 0;
    info->flat       = 0;
    info->resolution = 0;
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

        info->axis_map = lib_malloc(num_axes * sizeof *(info->axis_map));
        info->hat_map  = lib_malloc(num_hats * sizeof *(info->hat_map));

        num_axes = 0;
        num_hats = 0;
        for (code = ABS_X; code < ABS_RESERVED; code++) {
            if (libevdev_has_event_code(dev, EV_ABS, code)) {
                const struct input_absinfo *abs_evdev;
                joy_abs_info_t             *abs_vice;

                abs_evdev = libevdev_get_abs_info(dev, code);

                if (is_hat_code(code)) {
                    abs_vice = &(info->hat_map[num_hats]);
                    num_hats++;
                } else {
                    abs_vice = &(info->axis_map[num_axes]);
                    num_axes++;
                }

                abs_info_clear(abs_vice);
                abs_vice->code = (uint16_t)code;
                if (abs_evdev != NULL) {
                    abs_vice->minimum    = abs_evdev->minimum;
                    abs_vice->maximum    = abs_evdev->maximum;
                    abs_vice->fuzz       = abs_evdev->fuzz;
                    abs_vice->flat       = abs_evdev->flat;
                    abs_vice->resolution = abs_evdev->resolution;
                } else {
                    abs_vice->minimum = INT16_MIN;
                    abs_vice->maximum = INT16_MAX;
                }
            }
        }
    }
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
        /* num_hat * 2 since each hat consists of two axes */
        newdev->hat_map = lib_malloc(device->num_hats * 2u * sizeof *(newdev->hat_map));
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
    if (device != NULL) {dev_info_free_data(device);
        lib_free(device);
    }
}


static char *sd_get_full_path(const char *root, size_t root_len, const char *name)
{
    char   *fullpath;
    size_t  nlen = strlen(name);

    if (root == NULL || *root == '\0') {
        fullpath = lib_malloc(nlen + 2u);
        fullpath[0] = '/';
        memcpy(fullpath + 1, name, nlen + 1u);
    } else {
        size_t rlen;

        if (root[root_len - 1u] == '/') {
            rlen = root_len - 1u;
        } else {
            rlen = root_len;
        }
        fullpath = lib_malloc(rlen + nlen + 2u);
        memcpy(fullpath, root, rlen);
        fullpath[rlen] = '/';
        memcpy(fullpath + rlen + 1, name, nlen + 1u);
    }

    return fullpath;
}

static int sd_filter(const struct dirent *entry)
{
    const char *name;
    size_t      nlen;
    size_t      slen;

    name = entry->d_name;
    nlen = strlen(entry->d_name);
    slen = strlen(JOY_UDEV_SUFFIX);

    if ((nlen > slen) && memcmp(name + nlen - slen, JOY_UDEV_SUFFIX, slen) == 0) {
        return 1;
    }
    return 0;
}

static bool sd_get_dev_info(joy_dev_info_t *info)
{
    struct libevdev *dev;
    int              fd;
    int              rc;

    fd = open(info->path, O_RDONLY|O_NONBLOCK);
    if (fd < 0) {
        fprintf(stderr,
                "error: failed to open %s: %s\n",
                info->path, strerror(errno));
        return false;
    }
    rc = libevdev_new_from_fd(fd, &dev);
    if (rc < 0) {
        fprintf(stderr, "failed to initialize evdev: %s\n", strerror(-rc));
        close(fd);
        return false;
    }

    /* get device name */
    info->name = lib_strdup(libevdev_get_name(dev));

    /* get bus, vendor, product and version */
    info->bustype = (uint16_t)libevdev_get_id_bustype(dev);
    info->vendor  = (uint16_t)libevdev_get_id_vendor(dev);
    info->product = (uint16_t)libevdev_get_id_product(dev);
    info->version = (uint16_t)libevdev_get_id_version(dev);

    /* generate 128 bit GUID and string version thereof */
    dev_info_generate_guid(info);
    dev_info_generate_guid_str(info);

    dev_info_scan_buttons(info, dev);
    dev_info_scan_axes_and_hats(info, dev);

    libevdev_free(dev);
    close(fd);
    return true;
}


/** \brief  Scan connected joystick devices
 *
 * \param[in]   path    kernel virtual filesystem path with device nodes
 *
 * \return  number of devices found, or -1 on error
 */
int joy_scan_devices(const char *path, joy_dev_info_t ***devices)
{
    struct dirent **namelist;
    int             d;  /* device index */
    int             i;  /* info index */
    size_t          root_len;

    root_len = path != NULL ? strlen(path) : 0;

    if (devices_list != NULL) {
        /* previous list of scanned devices */
        joy_free_devices_list();
        devices_list = NULL;
    }

    devices_count = scandir(path == NULL ? "" : path, &namelist, sd_filter, NULL);
    if (devices_count < 0) {
        /* error */
        fprintf(stderr, "failed to scan devices: %s\n", strerror(errno));
        return -1;
    } else if (devices_count == 0) {
        if (devices != NULL) {
            *devices = NULL;
        }
        return 0;
    }

    devices_list = lib_malloc(((size_t)devices_count + 1u) * sizeof *devices_list);
    for (d = 0; d < devices_count + 1; d++) {
        devices_list[d] = NULL;
    }

    i = 0;
    for (d = 0; d < devices_count; d++) {
        joy_dev_info_t *info;

        info = lib_malloc(sizeof *info);
        dev_info_clear(info);
        info->path = sd_get_full_path(path, root_len, namelist[i]->d_name);

        if (sd_get_dev_info(info)) {
            devices_list[i++] = info;
        } else {
            lib_free(info->path);
            lib_free(info);
        }
        free(namelist[d]);
    }
    free(namelist);

    devices_list[i] = NULL;
    if (devices != NULL) {
        *devices = devices_list;
    }
    return devices_count;
}


/** \brief  Get devices list
 *
 * Get devices list generated by \c joy_scan_devices().
 *
 * \return  list of joystick devices
 */
joy_dev_info_t **joy_get_devices_list(void)
{
    return devices_list;
}


/** \brief  Get number of joystick devices
 *
* Get number of joystick devices as found by \c joy_scan_devices().
*
* \return   number of devices
*/
int joy_get_devices_count(void)
{
    return devices_count;
}


/** \brief  Free memory used by the devices list
 */
void joy_free_devices_list(void)
{
    if (devices_list != NULL) {
        int i;

        for (i = 0; i < devices_count; i++) {
            joy_dev_info_free(devices_list[i]);
        }
    }
    devices_list  = NULL;
    devices_count = 0;
}


static int compar_guid(const void *p1, const void *p2)
{
    const joy_dev_info_t *d1 = p1;
    const joy_dev_info_t *d2 = p2;

    return strcmp(d1->guid_str, d2->guid_str);
}

static int compar_name(const void *p1, const void *p2)
{
    const joy_dev_info_t *d1 = p1;
    const joy_dev_info_t *d2 = p2;

    return strcmp(d1->name, d2->name);
}

static int compar_node(const void *p1, const void *p2)
{
    const joy_dev_info_t *d1 = p1;
    const joy_dev_info_t *d2 = p2;

    return strcmp(d1->path, d2->path);
}


void joy_sort_devices_list(joy_sort_field_t field)
{
    int (*compar)(const void *, const void *);

    if (devices_list == NULL) {
        return;
    }
    switch (field) {
        case JOY_SORT_GUID:
            compar = compar_guid;
            break;
        case JOY_SORT_NAME:
            compar = compar_name;
            break;
        case JOY_SORT_NODE:
            compar = compar_node;
            break;
        default:
            return;
    }

    qsort((void *)devices_list, sizeof devices_list[0], sizeof devices_list, compar);
}
