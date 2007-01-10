# usb.rb - utility methods for libusb binding for Ruby.
#
# Copyright (C) 2007 Tanaka Akira
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public
# License as published by the Free Software Foundation; either
# version 2.1 of the License, or (at your option) any later version.
#
# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with this library; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA

require 'usb.so'

# USB module is a binding for libusb.
#
# = Example
#
# 1. list up USB devices
#
#   require 'usb'
#   require 'pp'
#   pp USB.devices
#   #=>
#   [#<USB::Device 001/001 0000:0000 Linux 2.6.17-2-486 uhci_hcd UHCI Host Controller 0000:00:1d.0 (Full speed Hub)>,
#    #<USB::Device 002/001 0000:0000 Linux 2.6.17-2-486 uhci_hcd UHCI Host Controller 0000:00:1d.1 (Full speed Hub)>,
#    #<USB::Device 003/001 0000:0000 Linux 2.6.17-2-486 uhci_hcd UHCI Host Controller 0000:00:1d.2 (Full speed Hub)>,
#    #<USB::Device 004/001 0000:0000 Linux 2.6.17-2-486 ehci_hcd EHCI Host Controller 0000:00:1d.7 (Hi-speed Hub with single TT)>]
#
# 2. find a device by bus id and device id
#
#   # find the device "004/001" in the above list.
#   dev = USB.find_bus(4).find_device(1)
#   p dev
#   #=>
#   #<USB::Device 004/001 0000:0000 Linux 2.6.17-2-486 ehci_hcd EHCI Host Controller 0000:00:1d.7 (Hi-speed Hub with single TT)>
#
# 3. open a device
#
#   dev.open {|handle| p handle }
#   #=>
#   #<USB::DevHandle:0xa7d94688>
#
# = USB overview
#
# * A host has busses.
# * A bus has devices.
# * A device has configurations.
# * A configuration has interfaces.
# * A interface has settings.
# * A setting has endpoints.
#

module USB
  def USB.busses
    result = []
    bus = USB.first_bus
    while bus
      result << bus
      bus = bus.next
    end
    result.sort_by {|b| b.dirname }
  end

  def USB.devices() USB.busses.map {|b| b.devices }.flatten end
  def USB.configurations() USB.devices.map {|d| d.configurations }.flatten end
  def USB.interfaces() USB.configurations.map {|d| d.interfaces }.flatten end
  def USB.settings() USB.interfaces.map {|d| d.settings }.flatten end
  def USB.endpoints() USB.settings.map {|d| d.endpoints }.flatten end

  def USB.find_bus(n)
    bus = USB.first_bus
    while bus
      return bus if n == bus.dirname.to_i
      bus = bus.next
    end
    return nil
  end

  # searches devices by USB class, subclass and protocol.
  #
  #   # find hubs.
  #   USB.each_device_by_class(USB::USB_CLASS_HUB) {|d| p d }'
  #
  #   # find Full speed Hubs
  #   USB.each_device_by_class(USB::USB_CLASS_HUB, 0, 0) {|d| p d }'
  #
  #   # find Hi-speed Hubs with single TT
  #   USB.each_device_by_class(USB::USB_CLASS_HUB, 0, 1) {|d| p d }'
  #
  #   # find Hi-speed Hubs with multiple TT
  #   USB.each_device_by_class(USB::USB_CLASS_HUB, 0, 2) {|d| p d }'
  #
  def USB.each_device_by_class(devclass, subclass=nil, protocol=nil)
    USB.devices.each {|dev|
      if dev.bDeviceClass == USB::USB_CLASS_PER_INTERFACE
        found = dev.settings.any? {|s|
                  s.bInterfaceClass == devclass &&
                  (!subclass || s.bInterfaceSubClass == subclass) &&
                  (!protocol || s.bInterfaceProtocol == protocol) }
      else
        found = dev.bDeviceClass == devclass &&
                (!subclass || dev.bDeviceSubClass == subclass) &&
                (!protocol || dev.bDeviceProtocol == protocol)
      end
      yield dev if found
    }
    nil
  end

  class Bus
    def inspect
      if self.revoked?
        "\#<#{self.class} revoked>"
      else
        "\#<#{self.class} #{self.dirname}>"
      end
    end

    def devices
      result = []
      device = self.first_device
      while device
        result << device
        device = device.next
      end
      result.sort_by {|d| d.filename }
    end

    def configurations() self.devices.map {|d| d.configurations }.flatten end
    def interfaces() self.configurations.map {|d| d.interfaces }.flatten end
    def settings() self.interfaces.map {|d| d.settings }.flatten end
    def endpoints() self.settings.map {|d| d.endpoints }.flatten end

    def find_device(n)
      device = self.first_device
      while device
        return device if n == device.filename.to_i
        device = device.next
      end
      return nil
    end
  end

  # :stopdoc:
  # http://www.usb.org/developers/defined_class
  CLASS_CODES = [
    [0x01, nil, nil, "Audio"],
    [0x02, nil, nil, "Comm"],
    [0x03, nil, nil, "HID"],
    [0x05, nil, nil, "Physical"],
    [0x06, 0x01, 0x01, "StillImaging"],
    [0x06, nil, nil, "Image"],
    [0x07, nil, nil, "Printer"],
    [0x08, 0x01, nil, "MassStorage RBC Bluk-Only"],
    [0x08, 0x02, 0x50, "MassStorage ATAPI Bluk-Only"],
    [0x08, 0x03, 0x50, "MassStorage QIC-157 Bluk-Only"],
    [0x08, 0x04, nil, "MassStorage UFI"],
    [0x08, 0x05, 0x50, "MassStorage SFF-8070i Bluk-Only"],
    [0x08, 0x06, 0x50, "MassStorage SCSI Bluk-Only"],
    [0x08, nil, nil, "MassStorage"],
    [0x09, 0x00, 0x00, "Full speed Hub"],
    [0x09, 0x00, 0x01, "Hi-speed Hub with single TT"],
    [0x09, 0x00, 0x02, "Hi-speed Hub with multiple TTs"],
    [0x09, nil, nil, "Hub"],
    [0x0a, nil, nil, "CDC"],
    [0x0b, nil, nil, "SmartCard"],
    [0x0d, 0x00, 0x00, "ContentSecurity"],
    [0x0e, nil, nil, "Video"],
    [0xdc, 0x01, 0x01, "Diagnostic USB2"],
    [0xdc, nil, nil, "Diagnostic"],
    [0xe0, 0x01, 0x01, "Bluetooth"],
    [0xe0, 0x01, 0x02, "UWB"],
    [0xe0, 0x01, 0x03, "RemoteNDIS"],
    [0xe0, 0x02, 0x01, "Host Wire Adapter Control/Data"],
    [0xe0, 0x02, 0x02, "Device Wire Adapter Control/Data"],
    [0xe0, 0x02, 0x03, "Device Wire Adapter Isochronous"],
    [0xe0, nil, nil, "Wireless Controller"],
    [0xef, 0x01, 0x01, "Active Sync"],
    [0xef, 0x01, 0x02, "Palm Sync"],
    [0xef, 0x02, 0x01, "Interface Association Descriptor"],
    [0xef, 0x02, 0x02, "Wire Adapter Multifunction Peripheral"],
    [0xef, 0x03, 0x01, "Cable Based Association Framework"],
    [0xef, nil, nil, "Miscellaneous"],
    [0xfe, 0x01, 0x01, "Device Firmware Upgrade"],
    [0xfe, 0x02, 0x00, "IRDA Bridge"],
    [0xfe, 0x03, 0x00, "USB Test and Measurement"],
    [0xfe, 0x03, 0x01, "USB Test and Measurement (USBTMC USB488)"],
    [0xfe, nil, nil, "Application Specific"],
    [0xff, nil, nil, "Vendor specific"],
  ]
  CLASS_CODES_HASH1 = {}
  CLASS_CODES_HASH2 = {}
  CLASS_CODES_HASH3 = {}
  CLASS_CODES.each {|base_class, sub_class, protocol, desc|
    if protocol
      CLASS_CODES_HASH3[[base_class, sub_class, protocol]] = desc
    elsif sub_class
      CLASS_CODES_HASH2[[base_class, sub_class]] = desc
    else
      CLASS_CODES_HASH1[base_class] = desc
    end
  }

  def USB.dev_string(base_class, sub_class, protocol)
    if desc = CLASS_CODES_HASH3[[base_class, sub_class, protocol]]
      desc
    elsif desc = CLASS_CODES_HASH2[[base_class, sub_class]]
      desc + " (%02x)" % [protocol]
    elsif desc = CLASS_CODES_HASH1[base_class]
      desc + " (%02x,%02x)" % [sub_class, protocol]
    else
      "Unkonwn(%02x,%02x,%02x)" % [base_class, sub_class, protocol]
    end
  end
  # :startdoc:

  class Device
    def inspect
      if self.revoked?
        "\#<#{self.class} revoked>"
      else
        attrs = []
        attrs << "#{self.bus.dirname}/#{self.filename}"
        attrs << ("%04x:%04x" % [self.idVendor, self.idProduct])
        attrs << self.manufacturer
        attrs << self.product
        attrs << self.serial_number
        if self.bDeviceClass == USB::USB_CLASS_PER_INTERFACE
          devclass = self.settings.map {|i|
            USB.dev_string(i.bInterfaceClass, i.bInterfaceSubClass, i.bInterfaceProtocol)
          }.join(", ")
        else
          devclass = USB.dev_string(self.bDeviceClass, self.bDeviceSubClass, self.bDeviceProtocol)
        end
        attrs << "(#{devclass})"
        attrs.compact!
        "\#<#{self.class} #{attrs.join(' ')}>"
      end
    end

    def manufacturer
      return @manufacturer if defined? @manufacturer
      @manufacturer = self.open {|h| h.get_string_simple(self.iManufacturer) }
      @manufacturer.strip! if @manufacturer
      @manufacturer
    end

    def product
      return @product if defined? @product
      @product = self.open {|h| h.get_string_simple(self.iProduct) }
      @product.strip! if @product
      @product
    end

    def serial_number
      return @serial_number if defined? @serial_number
      @serial_number = self.open {|h| h.get_string_simple(self.iSerialNumber) }
      @serial_number.strip! if @serial_number
      @serial_number
    end

    def open
      h = self.usb_open
      if block_given?
        begin
          r = yield h
        ensure
          h.usb_close
        end
      else
        h
      end
    end

    def interfaces() self.configurations.map {|d| d.interfaces }.flatten end
    def settings() self.interfaces.map {|d| d.settings }.flatten end
    def endpoints() self.settings.map {|d| d.endpoints }.flatten end
  end

  class Configuration
    def inspect
      if self.revoked?
        "\#<#{self.class} revoked>"
      else
        attrs = []
        attrs << self.bConfigurationValue.to_s
        bits = self.bmAttributes
        attrs << "SelfPowered" if (bits & 0b1000000) != 0
        attrs << "RemoteWakeup" if (bits & 0b100000) != 0
        desc = self.description
        attrs << desc if desc != '?'
        "\#<#{self.class} #{attrs.join(' ')}>"
      end
    end

    def description
      return @description if defined? @description
      @description = self.device.open {|h| h.get_string_simple(self.iConfiguration) }
    end

    def bus() self.device.bus end

    def settings() self.interfaces.map {|d| d.settings }.flatten end
    def endpoints() self.settings.map {|d| d.endpoints }.flatten end
  end

  class Interface
    def inspect
      if self.revoked?
        "\#<#{self.class} revoked>"
      else
        "\#<#{self.class}>"
      end
    end

    def bus() self.configuration.device.bus end
    def device() self.configuration.device end

    def endpoints() self.settings.map {|d| d.endpoints }.flatten end
  end

  class Setting
    def inspect
      if self.revoked?
        "\#<#{self.class} revoked>"
      else
        attrs = []
        attrs << self.bAlternateSetting.to_s
        devclass = USB.dev_string(self.bInterfaceClass, self.bInterfaceSubClass, self.bInterfaceProtocol)
        attrs << devclass
        desc = self.description
        attrs << desc if desc != '?'
        "\#<#{self.class} #{attrs.join(' ')}>"
      end
    end

    def description
      return @description if defined? @description
      @description = self.device.open {|h| h.get_string_simple(self.iInterface) }
    end

    def bus() self.interface.configuration.device.bus end
    def device() self.interface.configuration.device end
    def configuration() self.interface.configuration end
  end

  class Endpoint
    def inspect
      if self.revoked?
        "\#<#{self.class} revoked>"
      else
        endpoint_address = self.bEndpointAddress
        num = endpoint_address & 0b00001111
        inout = (endpoint_address & 0b10000000) == 0 ? "OUT" : "IN "
        bits = self.bmAttributes
        transfer_type = %w[Control Isochronous Bulk Interrupt][0b11 & bits]
        type = [transfer_type]
        if transfer_type == 'Isochronous'
          synchronization_type = %w[NoSynchronization Asynchronous Adaptive Synchronous][(0b1100 & bits) >> 2]
          usage_type = %w[Data Feedback ImplicitFeedback ?][(0b110000 & bits) >> 4]
          type << synchronization_type << usage_type
        end
        "\#<#{self.class} #{num} #{inout} #{type.join(" ")}>"
      end
    end

    def bus() self.setting.interface.configuration.device.bus end
    def device() self.setting.interface.configuration.device end
    def configuration() self.setting.interface.configuration end
    def interface() self.setting.interface end
  end

  class DevHandle
    def set_configuration(configuration)
      configuration = configuration.bConfigurationValue if configuration.respond_to? :bConfigurationValue
      self.usb_set_configuration(configuration)
    end

    def set_altinterface(alternate)
      alternate = alternate.bAlternateSetting if alternate.respond_to? :bAlternateSetting
      self.usb_set_altinterface(alternate)
    end

    def clear_halt(ep)
      ep = ep.bEndpointAddress if ep.respond_to? :bEndpointAddress
      self.usb_clear_halt(ep)
    end

    def claim_interface(interface)
      interface = interface.bInterfaceNumber if interface.respond_to? :bInterfaceNumber
      self.usb_claim_interface(interface)
    end

    def release_interface(interface)
      interface = interface.bInterfaceNumber if interface.respond_to? :bInterfaceNumber
      self.usb_release_interface(interface)
    end

    def get_string_simple(index)
      result = "\0" * 1024
      begin
        self.usb_get_string_simple(index, result)
      rescue Errno::EPIPE, Errno::EFBIG, Errno::EPERM
        return nil
      end
      result.delete!("\0")
      result
    end
  end
end
