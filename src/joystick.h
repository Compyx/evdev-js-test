/** \file   joystick.h
 * \brief   joystick interface using libevdev - header
 * \author  Bas Wassink <b.wassink@ziggo.nl>
 */

#ifndef JOYSTICK_H
#define JOYSTICK_H

#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <dirent.h>

#define JOY_INPUT_NODES_PATH    "/dev/input/by-id"

#define JOY_GUID_SIZE           16


typedef struct joy_abs_info_s {
    uint16_t code;
    int32_t  minimum;
    int32_t  maximum;
    int32_t  fuzz;
    int32_t  flat;
    int32_t  resolution;
} joy_abs_info_t;


typedef struct joy_dev_info_s {
    char           *path;           /**< evdev device node path */
    char           *name;           /**< evdev device name */

    uint8_t         guid[JOY_GUID_SIZE];              /**< 128-bit LE GUID */
    char            guid_str[JOY_GUID_SIZE * 2 + 1];  /**< GUID as hex string */

    uint16_t        bustype;        /**< evdev device bus type ID */
    uint16_t        vendor;         /**< evdev device vendor ID */
    uint16_t        product;        /**< evdev device product ID */
    uint16_t        version;        /**< evdev device version number */

    uint16_t        num_axes;       /**< number of axes */
    uint16_t        num_buttons;    /**< number of buttons */
    uint16_t        num_hats;       /**< number of hats */
    uint16_t        num_balls;      /**< number of balls */

    uint16_t       *button_map;     /**< button codes */
    joy_abs_info_t *axis_map;       /**< axis data */
    joy_abs_info_t *hat_map;        /**< axis data of the hats in X/Y order,
                                         so the size of this array is
                                         \c num_hats*2 */
} joy_dev_info_t;


typedef struct joy_dev_iter_s {
    DIR            *dirp;
    char           *root;
    size_t          root_len;
    bool            valid;
    joy_dev_info_t  device;
} joy_dev_iter_t;


bool             joy_dev_iter_init           (joy_dev_iter_t *iter, const char *path);
void             joy_dev_iter_free           (joy_dev_iter_t *iter);
bool             joy_dev_iter_next           (joy_dev_iter_t *iter);
joy_dev_info_t  *joy_dev_iter_get_device_info(joy_dev_iter_t *iter);

const char      *joy_get_axis_name(unsigned int code);
const char      *joy_get_button_name(unsigned int code);
const char      *joy_get_hat_name(unsigned int code);

joy_dev_info_t  *joy_dev_info_dup(const joy_dev_info_t *device);
void             joy_dev_info_free(joy_dev_info_t *device);

int              joy_scan_devices(const char *path, joy_dev_info_t ***devices);
joy_dev_info_t **joy_get_devices_list(void);
int              joy_get_devices_count(void);
void             joy_free_devices_list(void);

#endif
