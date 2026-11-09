#ifndef AT_DEVICE_H__
#define AT_DEVICE_H__
#include <rtthread.h>
#include <at.h>
#include <at_socket.h>
#include <arpa/inet.h>
#include <netdev.h>
struct at_device_class
{
    uint16_t class_id;
    const struct at_socket_ops *socket_ops;
    uint32_t socket_num;
};
struct at_device
{
    char name[RT_NAME_MAX];
    struct netdev *netdev;
    struct at_device_class *class;
    struct at_socket *sockets;
};
#define AT_DEVICE_NAMETYPE_NETDEV 2
struct at_device *at_device_get_first_initialized(void);
struct at_device *at_device_get_by_name(int type, const char *name);
#endif
