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

module USB
  def USB.busses
    result = []
    bus = USB.get_busses
    while bus
      result << bus
      bus = bus.next
    end
    result.sort_by {|b| b.dirname }
  end

  def USB.find_bus(n)
    bus = USB.get_busses
    while bus
      return bus if n == bus.dirname.to_i
      bus = bus.next
    end
    return nil
  end

  def USB.devices
    USB.busses.map {|b| b.devices }.flatten
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
      device = self.get_devices
      while device
        result << device
        device = device.next
      end
      result.sort_by {|d| d.filename }
    end

    def find_device(n)
      device = self.get_devices
      while device
        return device if n == device.filename.to_i
        device = device.next
      end
      return nil
    end
  end

  class Device
    def inspect
      if self.revoked?
        "\#<#{self.class} revoked>"
      else
        case self.descriptor_bDeviceClass
        when USB::USB_CLASS_PER_INTERFACE
          devclass = 'ClassPerInterface'
        when USB::USB_CLASS_AUDIO
          devclass = 'Audio'
        when USB::USB_CLASS_COMM
          devclass = 'Comm'
        when USB::USB_CLASS_HID
          devclass = 'HID'
        when USB::USB_CLASS_PRINTER
          devclass = 'Printer'
        when USB::USB_CLASS_PTP
          devclass = 'PTP'
        when USB::USB_CLASS_MASS_STORAGE
          devclass = 'MassStorage'
        when USB::USB_CLASS_HUB
          devclass = 'Hub'
        when USB::USB_CLASS_DATA
          devclass = 'Data'
        when USB::USB_CLASS_VENDOR_SPEC
          devclass = 'Vendor'
        else
          devclass = "Unknown(#{self.descriptor_bDeviceClass})"
        end
        "\#<#{self.class} #{self.bus.dirname}/#{self.filename} #{devclass}>"
      end
    end
  end

  class ConfigDescriptor
    def inspect
      if self.revoked?
        "\#<#{self.class} revoked>"
      else
        "\#<#{self.class}>"
      end
    end
  end

  class Interface
    def inspect
      if self.revoked?
        "\#<#{self.class} revoked>"
      else
        "\#<#{self.class}>"
      end
    end
  end
end
