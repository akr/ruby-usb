/*
   usb.c - libusb interface for Ruby.

   Copyright (C) 2007 Tanaka Akira

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with this library; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include "ruby.h"
#include <usb.h>
#include <errno.h>

#ifndef RSTRING_PTR
# define RSTRING_PTR(s) (RSTRING(s)->ptr)
# define RSTRING_LEN(s) (RSTRING(s)->len)
#endif

static VALUE rb_cUSB;

static VALUE rusb_dev_handle_new(usb_dev_handle *h);

#define define_usb_struct(c_name, ruby_name) \
  static VALUE rb_cUSB_ ## ruby_name; \
  static st_table *c_name ## _objects; \
  static void rusb_ ## c_name ## _free(void *p) {} \
  static VALUE rusb_ ## c_name ## _make(struct usb_ ## c_name *p) \
  { \
    VALUE v; \
    if (p == NULL) { return Qnil; } \
    if (st_lookup(c_name ## _objects, (st_data_t)p, (st_data_t *)&v)) \
      return v; \
    v = Data_Wrap_Struct(rb_cUSB_ ## ruby_name, 0, rusb_ ## c_name ## _free, p); \
    st_add_direct(c_name ## _objects, (st_data_t)p, (st_data_t)v); \
    return v; \
  } \
  static struct usb_ ## c_name *check_usb_ ## c_name(VALUE v) \
  { \
    Check_Type(v, T_DATA); \
    if (RDATA(v)->dfree != rusb_ ## c_name ## _free) { \
      rb_raise(rb_eTypeError, "wrong argument type %s (expected USB::" #ruby_name ")", \
               rb_class2name(CLASS_OF(v))); \
    } \
    return DATA_PTR(v); \
  } \
  static struct usb_ ## c_name *get_usb_ ## c_name(VALUE v) \
  { \
    struct usb_ ## c_name *p = check_usb_ ## c_name(v); \
    if (!p) { \
      rb_raise(rb_eArgError, "revoked USB::" #ruby_name); \
    }           \
    return p; \
  } 

define_usb_struct(bus, Bus)
define_usb_struct(device, Device)
define_usb_struct(config_descriptor, ConfigDescriptor)
define_usb_struct(interface, Interface)
define_usb_struct(interface_descriptor, InterfaceDescriptor)
define_usb_struct(endpoint_descriptor, EndpointDescriptor)

static int mark_data_i(st_data_t key, st_data_t val, st_data_t arg)
{
  if (DATA_PTR((VALUE)val))
    rb_gc_mark((VALUE)val);
  return ST_CONTINUE;
}

VALUE rusb_gc_root;
static void rusb_gc_mark(void *p) {
  st_foreach(bus_objects, mark_data_i, 0);
  st_foreach(device_objects, mark_data_i, 0);
  st_foreach(config_descriptor_objects, mark_data_i, 0);
  st_foreach(interface_objects, mark_data_i, 0);
}

/* -------- USB::Bus -------- */

static int revoke_data_i(st_data_t key, st_data_t val, st_data_t arg)
{
  DATA_PTR((VALUE)val) = NULL;
  return ST_DELETE;
}

/* USB.find_busses */
static VALUE
rusb_find_busses(VALUE cUSB)
{
  st_foreach(bus_objects, revoke_data_i, 0);
  st_foreach(device_objects, revoke_data_i, 0);
  st_foreach(config_descriptor_objects, revoke_data_i, 0);
  st_foreach(interface_objects, revoke_data_i, 0);
  return INT2NUM(usb_find_busses());
}

/* USB.find_devices */
static VALUE
rusb_find_devices(VALUE cUSB)
{
  return INT2NUM(usb_find_devices());
}

/* USB.get_busses */
static VALUE
rusb_get_busses(VALUE cUSB)
{
  struct usb_bus *bus = usb_get_busses();
  return rusb_bus_make(bus);
}

/* USB::Bus#inspect */
static VALUE
rusb_bus_inspect(VALUE vbus)
{
  struct usb_bus *bus = check_usb_bus(vbus);
  char *cname = rb_obj_classname(vbus);
  VALUE str;
  if (!bus) {
    return rb_sprintf("#<%s revoked>", cname);
  }
  str = rb_sprintf("#<%s ", cname);
  rb_str_cat2(str, bus->dirname);
  rb_str_cat2(str, ">");
  return str;
}

/* USB::Bus#prev */
static VALUE rusb_bus_prev(VALUE v) { return rusb_bus_make(get_usb_bus(v)->prev); }

/* USB::Bus#next */
static VALUE rusb_bus_next(VALUE v) { return rusb_bus_make(get_usb_bus(v)->next); }

/* USB::Bus#dirname */
static VALUE rusb_bus_dirname(VALUE v) { return rb_str_new2(get_usb_bus(v)->dirname); }

/* USB::Bus#location */
static VALUE rusb_bus_location(VALUE v) { return UINT2NUM(get_usb_bus(v)->location); }

/* USB::Bus#devices */
static VALUE rusb_bus_devices(VALUE v) { return rusb_device_make(get_usb_bus(v)->devices); }

/* -------- USB::Device -------- */

/* USB::Device#inspect */
static VALUE
rusb_device_inspect(VALUE vdevice)
{
  struct usb_device *device = check_usb_device(vdevice);
  char *cname = rb_obj_classname(vdevice);
  VALUE str;
  if (!device) {
    return rb_sprintf("#<%s revoked>", cname);
  }
  str = rb_sprintf("#<%s ", cname);
  rb_str_cat2(str, device->filename);
  rb_str_cat2(str, ">");
  return str;
}

/* USB::Device#prev */
static VALUE rusb_device_prev(VALUE v) { return rusb_device_make(get_usb_device(v)->prev); }

/* USB::Device#next */
static VALUE rusb_device_next(VALUE v) { return rusb_device_make(get_usb_device(v)->next); }

/* USB::Device#filename */
static VALUE rusb_device_filename(VALUE v) { return rb_str_new2(get_usb_device(v)->filename); }

/* USB::Device#bus */
static VALUE rusb_device_bus(VALUE v) { return rusb_bus_make(get_usb_device(v)->bus); }

/* USB::Device#devnum */
static VALUE rusb_device_devnum(VALUE v) { return INT2FIX(get_usb_device(v)->devnum); }

/* USB::Device#num_children */
static VALUE rusb_device_num_children(VALUE v) { return INT2FIX(get_usb_device(v)->num_children); }

/* USB::Device#children */
static VALUE
rusb_device_children(VALUE vdevice)
{
  struct usb_device *device = get_usb_device(vdevice);
  int i;
  VALUE children = rb_ary_new2(device->num_children);
  for (i = 0; i < device->num_children; i++)
    rb_ary_store(children, i, rusb_device_make(device->children[i]));
  return children;
}

/* USB::Device#descriptor_bLength */
static VALUE rusb_devdesc_bLength(VALUE v) { return INT2FIX(get_usb_device(v)->descriptor.bLength); }

/* USB::Device#descriptor_bDescriptorType */
static VALUE rusb_devdesc_bDescriptorType(VALUE v) { return INT2FIX(get_usb_device(v)->descriptor.bDescriptorType); }

/* USB::Device#descriptor_bcdUSB */
static VALUE rusb_devdesc_bcdUSB(VALUE v) { return INT2FIX(get_usb_device(v)->descriptor.bcdUSB); }

/* USB::Device#descriptor_bDeviceClass */
static VALUE rusb_devdesc_bDeviceClass(VALUE v) { return INT2FIX(get_usb_device(v)->descriptor.bDeviceClass); }

/* USB::Device#descriptor_bDeviceSubClass */
static VALUE rusb_devdesc_bDeviceSubClass(VALUE v) { return INT2FIX(get_usb_device(v)->descriptor.bDeviceSubClass); }

/* USB::Device#descriptor_bDeviceProtocol */
static VALUE rusb_devdesc_bDeviceProtocol(VALUE v) { return INT2FIX(get_usb_device(v)->descriptor.bDeviceProtocol); }

/* USB::Device#descriptor_bMaxPacketSize0 */
static VALUE rusb_devdesc_bMaxPacketSize0(VALUE v) { return INT2FIX(get_usb_device(v)->descriptor.bMaxPacketSize0); }

/* USB::Device#descriptor_idVendor */
static VALUE rusb_devdesc_idVendor(VALUE v) { return INT2FIX(get_usb_device(v)->descriptor.idVendor); }

/* USB::Device#descriptor_idProduct */
static VALUE rusb_devdesc_idProduct(VALUE v) { return INT2FIX(get_usb_device(v)->descriptor.idProduct); }

/* USB::Device#descriptor_bcdDevice */
static VALUE rusb_devdesc_bcdDevice(VALUE v) { return INT2FIX(get_usb_device(v)->descriptor.bcdDevice); }

/* USB::Device#descriptor_iManufacturer */
static VALUE rusb_devdesc_iManufacturer(VALUE v) { return INT2FIX(get_usb_device(v)->descriptor.iManufacturer); }

/* USB::Device#descriptor_iProduct */
static VALUE rusb_devdesc_iProduct(VALUE v) { return INT2FIX(get_usb_device(v)->descriptor.iProduct); }

/* USB::Device#descriptor_iSerialNumber */
static VALUE rusb_devdesc_iSerialNumber(VALUE v) { return INT2FIX(get_usb_device(v)->descriptor.iSerialNumber); }

/* USB::Device#descriptor_bNumConfigurations */
static VALUE rusb_devdesc_bNumConfigurations(VALUE v) { return INT2FIX(get_usb_device(v)->descriptor.bNumConfigurations); }

/* USB::Device#config */
static VALUE
rusb_device_config(VALUE vdevice)
{
  struct usb_device *device = get_usb_device(vdevice);
  int i;
  VALUE children = rb_ary_new2(device->descriptor.bNumConfigurations);
  for (i = 0; i < device->descriptor.bNumConfigurations; i++)
    rb_ary_store(children, i, rusb_config_descriptor_make(&device->config[i]));
  return children;
}

/* USB::Device#usb_open */
static VALUE
rusb_device_open(VALUE vdevice)
{
  struct usb_device *device = get_usb_device(vdevice);
  usb_dev_handle *h = usb_open(device);
  return rusb_dev_handle_new(h);
}

/* -------- USB::ConfigDescriptor -------- */

/* USB::ConfigDescriptor#inspect */
static VALUE
rusb_confdesc_inspect(VALUE vconfig_descriptor)
{
  struct usb_config_descriptor *config_descriptor = check_usb_config_descriptor(vconfig_descriptor);
  char *cname = rb_obj_classname(vconfig_descriptor);
  VALUE str;
  if (!config_descriptor) {
    return rb_sprintf("#<%s revoked>", cname);
  }
  str = rb_sprintf("#<%s ", cname);
  rb_str_cat2(str, ">");
  return str;
}

/* USB::ConfigDescriptor#bLength */
static VALUE rusb_confdesc_bLength(VALUE v) { return INT2FIX(get_usb_config_descriptor(v)->bLength); }

/* USB::ConfigDescriptor#bDescriptorType */
static VALUE rusb_confdesc_bDescriptorType(VALUE v) { return INT2FIX(get_usb_config_descriptor(v)->bDescriptorType); }

/* USB::ConfigDescriptor#wTotalLength */
static VALUE rusb_confdesc_wTotalLength(VALUE v) { return INT2FIX(get_usb_config_descriptor(v)->wTotalLength); }

/* USB::ConfigDescriptor#bNumInterfaces */
static VALUE rusb_confdesc_bNumInterfaces(VALUE v) { return INT2FIX(get_usb_config_descriptor(v)->bNumInterfaces); }

/* USB::ConfigDescriptor#bConfigurationValue */
static VALUE rusb_confdesc_bConfigurationValue(VALUE v) { return INT2FIX(get_usb_config_descriptor(v)->bConfigurationValue); }

/* USB::ConfigDescriptor#iConfiguration */
static VALUE rusb_confdesc_iConfiguration(VALUE v) { return INT2FIX(get_usb_config_descriptor(v)->iConfiguration); }

/* USB::ConfigDescriptor#bmAttributes */
static VALUE rusb_confdesc_bmAttributes(VALUE v) { return INT2FIX(get_usb_config_descriptor(v)->bmAttributes); }

/* USB::ConfigDescriptor#MaxPower */
static VALUE rusb_confdesc_MaxPower(VALUE v) { return INT2FIX(get_usb_config_descriptor(v)->MaxPower); }

/* USB::ConfigDescriptor#interface */
static VALUE
rusb_confdesc_interface(VALUE v)
{
  struct usb_config_descriptor *p = get_usb_config_descriptor(v);
  int i;
  VALUE interface = rb_ary_new2(p->bNumInterfaces);
  for (i = 0; i < p->bNumInterfaces; i++)
    rb_ary_store(interface, i, rusb_interface_make(&p->interface[i]));
  return interface;
}

/* -------- USB::Interface -------- */

/* USB::Interface#inspect */
static VALUE
rusb_interface_inspect(VALUE v)
{
  struct usb_interface *p = check_usb_interface(v);
  char *cname = rb_obj_classname(v);
  VALUE str;
  if (!p) {
    return rb_sprintf("#<%s revoked>", cname);
  }
  str = rb_sprintf("#<%s ", cname);
  rb_str_cat2(str, ">");
  return str;
}

/* USB::Interface#num_altsetting */
static VALUE rusb_interface_num_altsetting(VALUE v) { return INT2FIX(get_usb_interface(v)->num_altsetting); }

/* USB::Interface#altsetting */
static VALUE
rusb_interface_altsetting(VALUE v)
{
  struct usb_interface *p = get_usb_interface(v);
  int i;
  VALUE altsetting = rb_ary_new2(p->num_altsetting);
  for (i = 0; i <= p->num_altsetting; i++)
    rb_ary_store(altsetting, i, rusb_interface_descriptor_make(&p->altsetting[i]));
  return altsetting;
}

/* -------- USB::InterfaceDescriptor -------- */

/* USB::InterfaceDescriptor#bLength */
static VALUE rusb_ifdesc_bLength(VALUE v) { return INT2FIX(get_usb_interface_descriptor(v)->bLength); }

/* USB::InterfaceDescriptor#bDescriptorType */
static VALUE rusb_ifdesc_bDescriptorType(VALUE v) { return INT2FIX(get_usb_interface_descriptor(v)->bDescriptorType); }

/* USB::InterfaceDescriptor#bInterfaceNumber */
static VALUE rusb_ifdesc_bInterfaceNumber(VALUE v) { return INT2FIX(get_usb_interface_descriptor(v)->bInterfaceNumber); }

/* USB::InterfaceDescriptor#bAlternateSetting */
static VALUE rusb_ifdesc_bAlternateSetting(VALUE v) { return INT2FIX(get_usb_interface_descriptor(v)->bAlternateSetting); }

/* USB::InterfaceDescriptor#bNumEndpoints */
static VALUE rusb_ifdesc_bNumEndpoints(VALUE v) { return INT2FIX(get_usb_interface_descriptor(v)->bNumEndpoints); }

/* USB::InterfaceDescriptor#bInterfaceClass */
static VALUE rusb_ifdesc_bInterfaceClass(VALUE v) { return INT2FIX(get_usb_interface_descriptor(v)->bInterfaceClass); }

/* USB::InterfaceDescriptor#bInterfaceSubClass */
static VALUE rusb_ifdesc_bInterfaceSubClass(VALUE v) { return INT2FIX(get_usb_interface_descriptor(v)->bInterfaceSubClass); }

/* USB::InterfaceDescriptor#bInterfaceProtocol */
static VALUE rusb_ifdesc_bInterfaceProtocol(VALUE v) { return INT2FIX(get_usb_interface_descriptor(v)->bInterfaceProtocol); }

/* USB::InterfaceDescriptor#iInterface */
static VALUE rusb_ifdesc_iInterface(VALUE v) { return INT2FIX(get_usb_interface_descriptor(v)->iInterface); }

/* USB::InterfaceDescriptor#endpoint */
static VALUE
rusb_ifdesc_endpoint(VALUE v)
{
  struct usb_interface_descriptor *p = get_usb_interface_descriptor(v);
  int i;
  VALUE endpoint = rb_ary_new2(p->bNumEndpoints);
  for (i = 0; i < p->bNumEndpoints; i++)
    rb_ary_store(endpoint, i, rusb_endpoint_descriptor_make(&p->endpoint[i]));
  return endpoint;
}

/* -------- USB::EndpointDescriptor -------- */

/* USB::EndpointDescriptor#bLength */
static VALUE rusb_epdesc_bLength(VALUE v) { return INT2FIX(get_usb_endpoint_descriptor(v)->bLength); }

/* USB::EndpointDescriptor#bDescriptorType */
static VALUE rusb_epdesc_bDescriptorType(VALUE v) { return INT2FIX(get_usb_endpoint_descriptor(v)->bDescriptorType); }

/* USB::EndpointDescriptor#bEndpointAddress */
static VALUE rusb_epdesc_bEndpointAddress(VALUE v) { return INT2FIX(get_usb_endpoint_descriptor(v)->bEndpointAddress); }

/* USB::EndpointDescriptor#bmAttributes */
static VALUE rusb_epdesc_bmAttributes(VALUE v) { return INT2FIX(get_usb_endpoint_descriptor(v)->bmAttributes); }

/* USB::EndpointDescriptor#wMaxPacketSize */
static VALUE rusb_epdesc_wMaxPacketSize(VALUE v) { return INT2FIX(get_usb_endpoint_descriptor(v)->wMaxPacketSize); }

/* USB::EndpointDescriptor#bInterval */
static VALUE rusb_epdesc_bInterval(VALUE v) { return INT2FIX(get_usb_endpoint_descriptor(v)->bInterval); }

/* USB::EndpointDescriptor#bRefresh */
static VALUE rusb_epdesc_bRefresh(VALUE v) { return INT2FIX(get_usb_endpoint_descriptor(v)->bRefresh); }

/* USB::EndpointDescriptor#bSynchAddress */
static VALUE rusb_epdesc_bSynchAddress(VALUE v) { return INT2FIX(get_usb_endpoint_descriptor(v)->bSynchAddress); }

/* -------- USB::DevHandle -------- */

static VALUE rb_cUSB_DevHandle;

void rusb_devhandle_free(void *_h)
{
  usb_dev_handle *h = (usb_dev_handle *)_h;
  if (h) usb_close(h);
}

static VALUE
rusb_dev_handle_new(usb_dev_handle *h)
{
  return Data_Wrap_Struct(rb_cUSB_DevHandle, 0, rusb_devhandle_free, h);
}

static usb_dev_handle *check_usb_devhandle(VALUE v)
{
  Check_Type(v, T_DATA);
  if (RDATA(v)->dfree != rusb_devhandle_free) {
    rb_raise(rb_eTypeError, "wrong argument type %s (expected USB::DevHandle)",
             rb_class2name(CLASS_OF(v)));
  }
  return DATA_PTR(v);
}

static usb_dev_handle *get_usb_devhandle(VALUE v)
{
  usb_dev_handle *p = check_usb_devhandle(v); \
  if (!p) {
    rb_raise(rb_eArgError, "closed USB::DevHandle");
  }
  return p;
} 

static int check_usb_error(char *reason, int ret)
{
  if (ret < 0) {
    errno = -ret;
    rb_sys_fail(reason);
  }
  return ret;
}

/* USB::DevHandle#usb_close */
static VALUE
rusb_close(VALUE v)
{
  usb_dev_handle *p = get_usb_devhandle(v);
  check_usb_error("usb_close", usb_close(p));
  DATA_PTR(v) = NULL;
  return Qnil;
}

/* USB::DevHandle#usb_set_configuration(configuration) */
static VALUE
rusb_set_configuration(VALUE v, VALUE configuration)
{
  usb_dev_handle *p = get_usb_devhandle(v);
  int ret = usb_set_configuration(p, NUM2INT(configuration));
  check_usb_error("usb_set_configuration", ret);
  return Qnil;
}

/* USB::DevHandle#usb_set_altinterface(alternate) */
static VALUE
rusb_set_altinterface(VALUE v, VALUE alternate)
{
  usb_dev_handle *p = get_usb_devhandle(v);
  int ret = usb_set_altinterface(p, NUM2INT(alternate));
  check_usb_error("usb_set_altinterface", ret);
  return Qnil;
}

/* USB::DevHandle#usb_clear_halt(endpoint) */
static VALUE
rusb_clear_halt(VALUE v, VALUE ep)
{
  usb_dev_handle *p = get_usb_devhandle(v);
  int ret = usb_clear_halt(p, NUM2UINT(ep));
  check_usb_error("usb_clear_halt", ret);
  return Qnil;
}

/* USB::DevHandle#usb_reset */
static VALUE
rusb_reset(VALUE v)
{
  usb_dev_handle *p = get_usb_devhandle(v);
  int ret = usb_reset(p);
  check_usb_error("usb_reset", ret);
  /* xxx: call usb_close? */
  return Qnil;
}

/* USB::DevHandle#usb_claim_interface(interface) */
static VALUE
rusb_claim_interface(VALUE v, VALUE interface)
{
  usb_dev_handle *p = get_usb_devhandle(v);
  int ret = usb_claim_interface(p, NUM2INT(interface));
  check_usb_error("usb_claim_interface", ret);
  return Qnil;
}

/* USB::DevHandle#usb_release_interface(interface) */
static VALUE
rusb_release_interface(VALUE v, VALUE interface)
{
  usb_dev_handle *p = get_usb_devhandle(v);
  int ret = usb_release_interface(p, NUM2INT(interface));
  check_usb_error("usb_release_interface", ret);
  return Qnil;
}

/* USB::DevHandle#usb_control_msg(requesttype, request, value, index, bytes, timeout) */
static VALUE
rusb_control_msg(
  VALUE v,
  VALUE vrequesttype,
  VALUE vrequest,
  VALUE vvalue,
  VALUE vindex,
  VALUE vbytes,
  VALUE vtimeout)
{
  usb_dev_handle *p = get_usb_devhandle(v);
  int requesttype = NUM2INT(vrequesttype);
  int request = NUM2INT(vrequest);
  int value = NUM2INT(vvalue);
  int index = NUM2INT(vindex);
  int timeout = NUM2INT(vtimeout);
  char *bytes;
  int size;
  int ret;
  StringValue(vbytes);
  rb_str_modify(vbytes);
  bytes = RSTRING_PTR(vbytes);
  size = RSTRING_LEN(vbytes);
  ret = usb_control_msg(p, requesttype, request, value, index, bytes, size, timeout);
  check_usb_error("usb_control_msg", ret);
  return INT2NUM(ret);
}

/* USB::DevHandle#usb_get_string(index, langid, buf) */
static VALUE
rusb_get_string(
  VALUE v,
  VALUE vindex,
  VALUE vlangid,
  VALUE vbuf)
{
  usb_dev_handle *p = get_usb_devhandle(v);
  int index = NUM2INT(vindex);
  int langid = NUM2INT(vlangid);
  char *buf;
  int buflen;
  int ret;
  StringValue(vbuf);
  rb_str_modify(vbuf);
  buf = RSTRING_PTR(vbuf);
  buflen = RSTRING_LEN(vbuf);
  ret = usb_get_string(p, index, langid, buf, buflen);
  check_usb_error("usb_get_string", ret);
  return INT2NUM(ret);
}

/* USB::DevHandle#usb_get_string_simple(index, buf) */
static VALUE
rusb_get_string_simple(
  VALUE v,
  VALUE vindex,
  VALUE vbuf)
{
  usb_dev_handle *p = get_usb_devhandle(v);
  int index = NUM2INT(vindex);
  char *buf;
  int buflen;
  int ret;
  StringValue(vbuf);
  rb_str_modify(vbuf);
  buf = RSTRING_PTR(vbuf);
  buflen = RSTRING_LEN(vbuf);
  ret = usb_get_string_simple(p, index, buf, buflen);
  check_usb_error("usb_get_string_simple", ret);
  return INT2NUM(ret);
}

/* USB::DevHandle#usb_get_descriptor(type, index, buf) */
static VALUE
rusb_get_descriptor(
  VALUE v,
  VALUE vtype,
  VALUE vindex,
  VALUE vbuf)
{
  usb_dev_handle *p = get_usb_devhandle(v);
  int type = NUM2INT(vtype);
  int index = NUM2INT(vindex);
  char *buf;
  int buflen;
  int ret;
  StringValue(vbuf);
  rb_str_modify(vbuf);
  buf = RSTRING_PTR(vbuf);
  buflen = RSTRING_LEN(vbuf);
  ret = usb_get_descriptor(p, type, index, buf, buflen);
  check_usb_error("usb_get_descriptor", ret);
  return INT2NUM(ret);
}

/* USB::DevHandle#usb_get_descriptor_by_endpoint(endpoint, type, index, buf) */
static VALUE
rusb_get_descriptor_by_endpoint(
  VALUE v,
  VALUE vep,
  VALUE vtype,
  VALUE vindex,
  VALUE vbuf)
{
  usb_dev_handle *p = get_usb_devhandle(v);
  int ep = NUM2INT(vep);
  int type = NUM2INT(vtype);
  int index = NUM2INT(vindex);
  char *buf;
  int buflen;
  int ret;
  StringValue(vbuf);
  rb_str_modify(vbuf);
  buf = RSTRING_PTR(vbuf);
  buflen = RSTRING_LEN(vbuf);
  ret = usb_get_descriptor_by_endpoint(p, ep, type, index, buf, buflen);
  check_usb_error("usb_get_descriptor_by_endpoint", ret);
  return INT2NUM(ret);
}

/* USB::DevHandle#usb_bulk_write(endpoint, bytes, timeout) */
static VALUE
rusb_bulk_write(
  VALUE v,
  VALUE vep,
  VALUE vbytes,
  VALUE vtimeout)
{
  usb_dev_handle *p = get_usb_devhandle(v);
  int ep = NUM2INT(vep);
  int timeout = NUM2INT(vtimeout);
  char *bytes;
  int size;
  int ret;
  StringValue(vbytes);
  bytes = RSTRING_PTR(vbytes);
  size = RSTRING_LEN(vbytes);
  ret = usb_bulk_write(p, ep, bytes, size, timeout);
  check_usb_error("usb_bulk_write", ret);
  return INT2NUM(ret);
}

/* USB::DevHandle#usb_bulk_read(endpoint, bytes, timeout) */
static VALUE
rusb_bulk_read(
  VALUE v,
  VALUE vep,
  VALUE vbytes,
  VALUE vtimeout)
{
  usb_dev_handle *p = get_usb_devhandle(v);
  int ep = NUM2INT(vep);
  int timeout = NUM2INT(vtimeout);
  char *bytes;
  int size;
  int ret;
  StringValue(vbytes);
  rb_str_modify(vbytes);
  bytes = RSTRING_PTR(vbytes);
  size = RSTRING_LEN(vbytes);
  ret = usb_bulk_read(p, ep, bytes, size, timeout);
  check_usb_error("usb_bulk_read", ret);
  return INT2NUM(ret);
}

/* USB::DevHandle#usb_interrupt_write(endpoint, bytes, timeout) */
static VALUE
rusb_interrupt_write(
  VALUE v,
  VALUE vep,
  VALUE vbytes,
  VALUE vtimeout)
{
  usb_dev_handle *p = get_usb_devhandle(v);
  int ep = NUM2INT(vep);
  int timeout = NUM2INT(vtimeout);
  char *bytes;
  int size;
  int ret;
  StringValue(vbytes);
  bytes = RSTRING_PTR(vbytes);
  size = RSTRING_LEN(vbytes);
  ret = usb_interrupt_write(p, ep, bytes, size, timeout);
  check_usb_error("usb_interrupt_write", ret);
  return INT2NUM(ret);
}

/* USB::DevHandle#usb_interrupt_read(endpoint, bytes, timeout) */
static VALUE
rusb_interrupt_read(
  VALUE v,
  VALUE vep,
  VALUE vbytes,
  VALUE vtimeout)
{
  usb_dev_handle *p = get_usb_devhandle(v);
  int ep = NUM2INT(vep);
  int timeout = NUM2INT(vtimeout);
  char *bytes;
  int size;
  int ret;
  StringValue(vbytes);
  rb_str_modify(vbytes);
  bytes = RSTRING_PTR(vbytes);
  size = RSTRING_LEN(vbytes);
  ret = usb_interrupt_read(p, ep, bytes, size, timeout);
  check_usb_error("usb_interrupt_read", ret);
  return INT2NUM(ret);
}

#ifdef LIBUSB_HAS_GET_DRIVER_NP
/* USB::DevHandle#usb_get_driver_np(interface, name) */
static VALUE
rusb_get_driver_np(
  VALUE v,
  VALUE vinterface,
  VALUE vname)
{
  usb_dev_handle *p = get_usb_devhandle(v);
  int interface = NUM2INT(vinterface);
  char *name;
  int namelen;
  int ret;
  StringValue(vname);
  rb_str_modify(vname);
  name = RSTRING_PTR(vname);
  namelen = RSTRING_LEN(vname);
  ret = usb_get_driver_np(p, interface, name, namelen);
  check_usb_error("usb_get_driver_np", ret);
  return Qnil;
}
#endif

#ifdef LIBUSB_HAS_DETACH_KERNEL_DRIVER_NP
/* USB::DevHandle#usb_detach_kernel_driver_np(interface) */
static VALUE
rusb_detach_kernel_driver_np(
  VALUE v,
  VALUE vinterface)
{
  usb_dev_handle *p = get_usb_devhandle(v);
  int interface = NUM2INT(vinterface);
  int ret;
  ret = usb_detach_kernel_driver_np(p, interface);
  check_usb_error("usb_detach_kernel_driver_np", ret);
  return Qnil;
}
#endif

/* -------- libusb binding initialization -------- */

void
Init_usb()
{
  rb_cUSB = rb_define_module("USB");

#define f(name) rb_define_const(rb_cUSB, #name, INT2NUM(name));
#include "constants.h"
#undef f

  bus_objects = st_init_numtable();
  rb_cUSB_Bus = rb_define_class_under(rb_cUSB, "Bus", rb_cData);

  device_objects = st_init_numtable();
  rb_cUSB_Device = rb_define_class_under(rb_cUSB, "Device", rb_cData);

  config_descriptor_objects = st_init_numtable();
  rb_cUSB_ConfigDescriptor = rb_define_class_under(rb_cUSB, "ConfigDescriptor", rb_cData);

  interface_objects = st_init_numtable();
  rb_cUSB_Interface = rb_define_class_under(rb_cUSB, "Interface", rb_cData);

  interface_descriptor_objects = st_init_numtable();
  rb_cUSB_InterfaceDescriptor = rb_define_class_under(rb_cUSB, "InterfaceDescriptor", rb_cData);

  endpoint_descriptor_objects = st_init_numtable();
  rb_cUSB_EndpointDescriptor = rb_define_class_under(rb_cUSB, "EndpointDescriptor", rb_cData);

  rb_cUSB_DevHandle = rb_define_class_under(rb_cUSB, "DevHandle", rb_cData);

  rusb_gc_root = Data_Wrap_Struct(0, rusb_gc_mark, 0, 0); \
  rb_global_variable(&rusb_gc_root);

  usb_init();
  usb_find_busses(); /* xxx: return value */
  usb_find_devices(); /* xxx: return value */

  rb_define_module_function(rb_cUSB, "find_busses", rusb_find_busses, 0);
  rb_define_module_function(rb_cUSB, "find_devices", rusb_find_devices, 0);
  rb_define_module_function(rb_cUSB, "get_busses", rusb_get_busses, 0);

  rb_define_method(rb_cUSB_Bus, "inspect", rusb_bus_inspect, 0);
  rb_define_method(rb_cUSB_Bus, "prev", rusb_bus_prev, 0);
  rb_define_method(rb_cUSB_Bus, "next", rusb_bus_next, 0);
  rb_define_method(rb_cUSB_Bus, "dirname", rusb_bus_dirname, 0);
  rb_define_method(rb_cUSB_Bus, "location", rusb_bus_location, 0);
  rb_define_method(rb_cUSB_Bus, "devices", rusb_bus_devices, 0);

  rb_define_method(rb_cUSB_Device, "inspect", rusb_device_inspect, 0);
  rb_define_method(rb_cUSB_Device, "prev", rusb_device_prev, 0);
  rb_define_method(rb_cUSB_Device, "next", rusb_device_next, 0);
  rb_define_method(rb_cUSB_Device, "filename", rusb_device_filename, 0);
  rb_define_method(rb_cUSB_Device, "bus", rusb_device_bus, 0);
  rb_define_method(rb_cUSB_Device, "devnum", rusb_device_devnum, 0);
  rb_define_method(rb_cUSB_Device, "num_children", rusb_device_num_children, 0);
  rb_define_method(rb_cUSB_Device, "children", rusb_device_children, 0);
  rb_define_method(rb_cUSB_Device, "descriptor_bLength", rusb_devdesc_bLength, 0);
  rb_define_method(rb_cUSB_Device, "descriptor_bDescriptorType", rusb_devdesc_bDescriptorType, 0);
  rb_define_method(rb_cUSB_Device, "descriptor_bcdUSB", rusb_devdesc_bcdUSB, 0);
  rb_define_method(rb_cUSB_Device, "descriptor_bDeviceClass", rusb_devdesc_bDeviceClass, 0);
  rb_define_method(rb_cUSB_Device, "descriptor_bDeviceSubClass", rusb_devdesc_bDeviceSubClass, 0);
  rb_define_method(rb_cUSB_Device, "descriptor_bDeviceProtocol", rusb_devdesc_bDeviceProtocol, 0);
  rb_define_method(rb_cUSB_Device, "descriptor_bMaxPacketSize0", rusb_devdesc_bMaxPacketSize0, 0);
  rb_define_method(rb_cUSB_Device, "descriptor_idVendor", rusb_devdesc_idVendor, 0);
  rb_define_method(rb_cUSB_Device, "descriptor_idProduct", rusb_devdesc_idProduct, 0);
  rb_define_method(rb_cUSB_Device, "descriptor_bcdDevice", rusb_devdesc_bcdDevice, 0);
  rb_define_method(rb_cUSB_Device, "descriptor_iManufacturer", rusb_devdesc_iManufacturer, 0);
  rb_define_method(rb_cUSB_Device, "descriptor_iProduct", rusb_devdesc_iProduct, 0);
  rb_define_method(rb_cUSB_Device, "descriptor_iSerialNumber", rusb_devdesc_iSerialNumber, 0);
  rb_define_method(rb_cUSB_Device, "descriptor_bNumConfigurations", rusb_devdesc_bNumConfigurations, 0);
  rb_define_method(rb_cUSB_Device, "config", rusb_device_config, 0);
  rb_define_method(rb_cUSB_Device, "usb_open", rusb_device_open, 0);

  rb_define_method(rb_cUSB_ConfigDescriptor, "inspect", rusb_confdesc_inspect, 0);
  rb_define_method(rb_cUSB_ConfigDescriptor, "bLength", rusb_confdesc_bLength, 0);
  rb_define_method(rb_cUSB_ConfigDescriptor, "bDescriptorType", rusb_confdesc_bDescriptorType, 0);
  rb_define_method(rb_cUSB_ConfigDescriptor, "wTotalLength", rusb_confdesc_wTotalLength, 0);
  rb_define_method(rb_cUSB_ConfigDescriptor, "bNumInterfaces", rusb_confdesc_bNumInterfaces, 0);
  rb_define_method(rb_cUSB_ConfigDescriptor, "bConfigurationValue", rusb_confdesc_bConfigurationValue, 0);
  rb_define_method(rb_cUSB_ConfigDescriptor, "iConfiguration", rusb_confdesc_iConfiguration, 0);
  rb_define_method(rb_cUSB_ConfigDescriptor, "bmAttributes", rusb_confdesc_bmAttributes, 0);
  rb_define_method(rb_cUSB_ConfigDescriptor, "MaxPower", rusb_confdesc_MaxPower, 0);
  rb_define_method(rb_cUSB_ConfigDescriptor, "interface", rusb_confdesc_interface, 0);

  rb_define_method(rb_cUSB_Interface, "inspect", rusb_interface_inspect, 0);
  rb_define_method(rb_cUSB_Interface, "num_altsetting", rusb_interface_num_altsetting, 0);
  rb_define_method(rb_cUSB_Interface, "altsetting", rusb_interface_altsetting, 0);

  rb_define_method(rb_cUSB_InterfaceDescriptor, "bLength", rusb_ifdesc_bLength, 0);
  rb_define_method(rb_cUSB_InterfaceDescriptor, "bDescriptorType", rusb_ifdesc_bDescriptorType, 0);
  rb_define_method(rb_cUSB_InterfaceDescriptor, "bInterfaceNumber", rusb_ifdesc_bInterfaceNumber, 0);
  rb_define_method(rb_cUSB_InterfaceDescriptor, "bAlternateSetting", rusb_ifdesc_bAlternateSetting, 0);
  rb_define_method(rb_cUSB_InterfaceDescriptor, "bNumEndpoints", rusb_ifdesc_bNumEndpoints, 0);
  rb_define_method(rb_cUSB_InterfaceDescriptor, "bInterfaceClass", rusb_ifdesc_bInterfaceClass, 0);
  rb_define_method(rb_cUSB_InterfaceDescriptor, "bInterfaceSubClass", rusb_ifdesc_bInterfaceSubClass, 0);
  rb_define_method(rb_cUSB_InterfaceDescriptor, "bInterfaceProtocol", rusb_ifdesc_bInterfaceProtocol, 0);
  rb_define_method(rb_cUSB_InterfaceDescriptor, "iInterface", rusb_ifdesc_iInterface, 0);
  rb_define_method(rb_cUSB_InterfaceDescriptor, "endpoint", rusb_ifdesc_endpoint, 0);

  rb_define_method(rb_cUSB_EndpointDescriptor, "bLength", rusb_epdesc_bLength, 0);
  rb_define_method(rb_cUSB_EndpointDescriptor, "bDescriptorType", rusb_epdesc_bDescriptorType, 0);
  rb_define_method(rb_cUSB_EndpointDescriptor, "bEndpointAddress", rusb_epdesc_bEndpointAddress, 0);
  rb_define_method(rb_cUSB_EndpointDescriptor, "bmAttributes", rusb_epdesc_bmAttributes, 0);
  rb_define_method(rb_cUSB_EndpointDescriptor, "wMaxPacketSize", rusb_epdesc_wMaxPacketSize, 0);
  rb_define_method(rb_cUSB_EndpointDescriptor, "bInterval", rusb_epdesc_bInterval, 0);
  rb_define_method(rb_cUSB_EndpointDescriptor, "bRefresh", rusb_epdesc_bRefresh, 0);
  rb_define_method(rb_cUSB_EndpointDescriptor, "bSynchAddress", rusb_epdesc_bSynchAddress, 0);

  rb_define_method(rb_cUSB_DevHandle, "usb_close", rusb_close, 0);
  rb_define_method(rb_cUSB_DevHandle, "usb_set_configuration", rusb_set_configuration, 1);
  rb_define_method(rb_cUSB_DevHandle, "usb_set_altinterface", rusb_set_altinterface, 1);
  rb_define_method(rb_cUSB_DevHandle, "usb_clear_halt", rusb_clear_halt, 1);
  rb_define_method(rb_cUSB_DevHandle, "usb_reset", rusb_reset, 0);
  rb_define_method(rb_cUSB_DevHandle, "usb_claim_interface", rusb_claim_interface, 1);
  rb_define_method(rb_cUSB_DevHandle, "usb_release_interface", rusb_release_interface, 1);
  rb_define_method(rb_cUSB_DevHandle, "usb_control_msg", rusb_control_msg, 6);
  rb_define_method(rb_cUSB_DevHandle, "usb_get_string", rusb_get_string, 3);
  rb_define_method(rb_cUSB_DevHandle, "usb_get_string_simple", rusb_get_string_simple, 2);
  rb_define_method(rb_cUSB_DevHandle, "usb_get_descriptor", rusb_get_descriptor, 3);
  rb_define_method(rb_cUSB_DevHandle, "usb_get_descriptor_by_endpoint", rusb_get_descriptor_by_endpoint, 4);
  rb_define_method(rb_cUSB_DevHandle, "usb_bulk_write", rusb_bulk_write, 3);
  rb_define_method(rb_cUSB_DevHandle, "usb_bulk_read", rusb_bulk_read, 3);
  rb_define_method(rb_cUSB_DevHandle, "usb_interrupt_write", rusb_interrupt_write, 3);
  rb_define_method(rb_cUSB_DevHandle, "usb_interrupt_read", rusb_interrupt_read, 3);

#ifdef LIBUSB_HAS_GET_DRIVER_NP
  rb_define_method(rb_cUSB_DevHandle, "usb_get_driver_np", rusb_get_driver_np, 2);
#endif
#ifdef LIBUSB_HAS_DETACH_KERNEL_DRIVER_NP
  rb_define_method(rb_cUSB_DevHandle, "usb_detach_kernel_driver_np", rusb_detach_kernel_driver_np, 2);
#endif
}
